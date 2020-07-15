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
    getVideoMixer()->setOnSourceRendered([this](Observable<std::shared_ptr<MediaFrame>>* frame, int, int, int, int) {
        std::lock_guard<std::mutex> lk(videToCallMtx_);
        auto it = videoToCall_.find(frame);
        if (it != videoToCall_.end()) {
            JAMI_ERR("@@@ FOUND");
        }
    });
    std::lock_guard<std::mutex> lk(videToCallMtx_);
    videoToCall_.emplace(getVideoCamera().get(), std::string());
}

Conference::~Conference()
{
#ifdef ENABLE_VIDEO
    for (const auto &participant_id : participants_) {
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
Conference::add(const std::string &participant_id)
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
Conference::setActiveParticipant(const std::string &participant_id)
{
    for (const auto &item : participants_) {
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

void
Conference::sendConferenceInfo()
{
    for (const auto &participant_id : participants_) {
        if (auto call = Manager::instance().callFactory.getCall<SIPCall>(participant_id)) {
            JAMI_WARN("@@@ TODO");
        }
    }
}

void
Conference::attachVideo(Observable<std::shared_ptr<MediaFrame>>* frame, const std::string& callId)
{
    std::lock_guard<std::mutex> lk(videToCallMtx_);
    videoToCall_.emplace(frame, callId);
}

void
Conference::detachVideo(Observable<std::shared_ptr<MediaFrame>>* frame)
{
    std::lock_guard<std::mutex> lk(videToCallMtx_);
    auto it = videoToCall_.find(frame);
    if (it != videoToCall_.end()) {
        it->first->detach(videoMixer_.get());
        videoToCall_.erase(it);
    }
}

void
Conference::remove(const std::string &participant_id)
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
Conference::bindParticipant(const std::string &participant_id)
{
    auto &rbPool = Manager::instance().getRingBufferPool();

    for (const auto &item : participants_) {
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

    for (const auto &p : participants_) {
        auto details = Manager::instance().getCallDetails(p);
        const auto tmp = details["DISPLAY_NAME"];
        result.emplace_back(tmp.empty() ? details["PEER_NUMBER"] : tmp);
    }
    return result;
}

bool Conference::toggleRecording()
{
    const bool startRecording = Recordable::toggleRecording();
    return startRecording;
}

const std::string&
Conference::getConfID() const {
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
std::shared_ptr<video::VideoMixer> Conference::getVideoMixer()
{
    if (!videoMixer_)
        videoMixer_.reset(new video::VideoMixer(id_));
    return videoMixer_;
}
#endif

} // namespace jami
