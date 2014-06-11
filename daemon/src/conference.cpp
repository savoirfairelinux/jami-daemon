/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author : Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include <sstream>

#include "conference.h"
#include "manager.h"
#include "audio/audiolayer.h"
#include "audio/mainbuffer.h"

#ifdef SFL_VIDEO
#include "sip/sipvoiplink.h"
#include "sip/sipcall.h"
#include "client/videomanager.h"
#include "video/video_input.h"
#endif

#include "logger.h"


Conference::Conference()
    : id_(Manager::instance().getNewCallID())
    , confState_(ACTIVE_ATTACHED)
    , participants_()
#ifdef SFL_VIDEO
    , videoMixer_(nullptr)
#endif
{
    Recordable::initRecFilename(id_);
}

Conference::~Conference()
{
#ifdef SFL_VIDEO
    for (const auto &participant_id : participants_) {
        if (auto call = SIPVoIPLink::instance().getSipCall(participant_id))
            call->getVideoRtp().exitConference();
    }
#endif // SFL_VIDEO
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
#ifdef SFL_VIDEO
        if (auto call = SIPVoIPLink::instance().getSipCall(participant_id))
            call->getVideoRtp().enterConference(this);
#endif // SFL_VIDEO
    }
}

void Conference::remove(const std::string &participant_id)
{
    if (participants_.erase(participant_id)) {
#ifdef SFL_VIDEO
        if (auto call = SIPVoIPLink::instance().getSipCall(participant_id))
            call->getVideoRtp().exitConference();
#endif // SFL_VIDEO
    }
}

void Conference::bindParticipant(const std::string &participant_id)
{
    auto &mainBuffer = Manager::instance().getMainBuffer();

    for (const auto &item : participants_) {
        if (participant_id != item)
            mainBuffer.bindCallID(participant_id, item);
        mainBuffer.flush(item);
    }

    mainBuffer.bindCallID(participant_id, MainBuffer::DEFAULT_ID);
    mainBuffer.flush(MainBuffer::DEFAULT_ID);
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
    MainBuffer &mbuffer = Manager::instance().getMainBuffer();

    std::string process_id(Recordable::recorder_.getRecorderID());

    // start recording
    if (startRecording) {
        for (const auto &item : participants_)
            mbuffer.bindHalfDuplexOut(process_id, item);

        mbuffer.bindHalfDuplexOut(process_id, MainBuffer::DEFAULT_ID);

        Recordable::recorder_.start();
    } else {
        for (const auto &item : participants_)
            mbuffer.unBindHalfDuplexOut(process_id, item);

        mbuffer.unBindHalfDuplexOut(process_id, MainBuffer::DEFAULT_ID);
    }

    return startRecording;
}

std::string Conference::getConfID() const {
    return id_;
}

#ifdef SFL_VIDEO
std::shared_ptr<sfl_video::VideoMixer> Conference::getVideoMixer()
{
    if (!videoMixer_)
        videoMixer_.reset(new sfl_video::VideoMixer(id_));
    return videoMixer_;
}
#endif
