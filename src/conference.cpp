/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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
{}

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

void Conference::setState(State state)
{
    confState_ = state;
}

void Conference::add(const std::string &participant_id)
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

void Conference::remove(const std::string &participant_id)
{
    if (participants_.erase(participant_id)) {
#ifdef ENABLE_VIDEO
        if (auto call = Manager::instance().callFactory.getCall<SIPCall>(participant_id))
            call->getVideoRtp().exitConference();
#endif // ENABLE_VIDEO
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

const ParticipantSet&
Conference::getParticipantList() const
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
