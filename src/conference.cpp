/*
 *  Copyright (C) 2004-2016 Savoir-faire Linux Inc.
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
#include "audio/audiorecord.h"

#ifdef RING_VIDEO
#include "sip/sipcall.h"
#include "client/videomanager.h"
#include "video/video_input.h"
#include "video/video_mixer.h"
#endif

#include "call_factory.h"

#include "logger.h"

namespace ring {

Conference::Conference()
    : id_(Manager::instance().getNewCallID())
    , confState_(ACTIVE_ATTACHED)
    , participants_()
#ifdef RING_VIDEO
    , videoMixer_(nullptr)
#endif
{
    Recordable::initRecFilename(id_);
}

Conference::~Conference()
{
#ifdef RING_VIDEO
    for (const auto &participant_id : participants_) {
        if (auto call = Manager::instance().callFactory.getCall<SIPCall>(participant_id))
            call->getVideoRtp().exitConference();
    }
#endif // RING_VIDEO
}

Conference::ConferenceState Conference::getState() const
{
    return confState_;
}

void Conference::setState(ConferenceState state)
{
    confState_ = state;
}

void Conference::add(const std::string &participant_id)
{
    if (participants_.insert(participant_id).second) {
#ifdef RING_VIDEO
        if (auto call = Manager::instance().callFactory.getCall<SIPCall>(participant_id))
            call->getVideoRtp().enterConference(this);
        else
            RING_ERR("no call associate to participant %s", participant_id.c_str());
#endif // RING_VIDEO
    }
}

void Conference::remove(const std::string &participant_id)
{
    if (participants_.erase(participant_id)) {
#ifdef RING_VIDEO
        if (auto call = Manager::instance().callFactory.getCall<SIPCall>(participant_id))
            call->getVideoRtp().exitConference();
#endif // RING_VIDEO
    }
}

void Conference::bindParticipant(const std::string &participant_id)
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

std::string Conference::getStateStr() const
{
    switch (confState_) {
        case ACTIVE_ATTACHED:
            return "ACTIVE_ATTACHED";
        case ACTIVE_DETACHED:
            return "ACTIVE_DETACHED";
        case ACTIVE_ATTACHED_REC:
            return "ACTIVE_ATTACHED_REC";
        case ACTIVE_DETACHED_REC:
            return "ACTIVE_DETACHED_REC";
        case HOLD:
            return "HOLD";
        case HOLD_REC:
            return "HOLD_REC";
        default:
            return "";
    }
}

ParticipantSet Conference::getParticipantList() const
{
    return participants_;
}

std::vector<std::string>
Conference::getDisplayNames() const
{
    std::vector<std::string> result;

    for (const auto &p : participants_) {
        auto details = Manager::instance().getCallDetails(p);
        const auto tmp = details["DISPLAY_NAME"];
        result.push_back(tmp.empty() ? details["PEER_NUMBER"] : tmp);
    }
    return result;
}

bool Conference::toggleRecording()
{
    const bool startRecording = Recordable::toggleRecording();
    std::string process_id(Recordable::recAudio_->getRecorderID());
    auto& rbPool = Manager::instance().getRingBufferPool();

    // start recording
    if (startRecording) {
        for (const auto &item : participants_)
            rbPool.bindHalfDuplexOut(process_id, item);

        rbPool.bindHalfDuplexOut(process_id, RingBufferPool::DEFAULT_ID);
    } else {
        for (const auto &item : participants_)
            rbPool.unBindHalfDuplexOut(process_id, item);

        rbPool.unBindHalfDuplexOut(process_id, RingBufferPool::DEFAULT_ID);
    }

    return startRecording;
}

std::string Conference::getConfID() const {
    return id_;
}

#ifdef RING_VIDEO
std::shared_ptr<video::VideoMixer> Conference::getVideoMixer()
{
    if (!videoMixer_)
        videoMixer_.reset(new video::VideoMixer(id_));
    return videoMixer_;
}
#endif

} // namespace ring
