/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

Conference::Conference()
    : id_(Manager::instance().getNewCallID())
    , confState_(ACTIVE_ATTACHED)
    , participants_()
{
    Recordable::initRecFilename(id_);
}

int Conference::getState() const
{
    return confState_;
}

void Conference::setState(ConferenceState state)
{
    confState_ = state;
}

void Conference::add(const std::string &participant_id)
{
    participants_.insert(participant_id);
}

void Conference::remove(const std::string &participant_id)
{
    participants_.erase(participant_id);
}

void Conference::bindParticipant(const std::string &participant_id)
{
    for (ParticipantSet::const_iterator iter = participants_.begin();
            iter != participants_.end(); ++iter)
        if (participant_id != *iter)
            Manager::instance().getMainBuffer()->bindCallID(participant_id, *iter);

    Manager::instance().getMainBuffer()->bindCallID(participant_id, MainBuffer::DEFAULT_ID);
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

bool Conference::setRecording()
{
    bool recordStatus = Recordable::recAudio_.isRecording();

    Recordable::recAudio_.setRecording();
    MainBuffer *mbuffer = Manager::instance().getMainBuffer();

    std::string process_id(Recordable::recorder_.getRecorderID());

    // start recording
    if (!recordStatus) {
        for (ParticipantSet::const_iterator iter = participants_.begin(); iter != participants_.end(); ++iter)
            mbuffer->bindHalfDuplexOut(process_id, *iter);

        mbuffer->bindHalfDuplexOut(process_id, MainBuffer::DEFAULT_ID);

        Recordable::recorder_.start();
    } else {
        for (ParticipantSet::const_iterator iter = participants_.begin(); iter != participants_.end(); ++iter)
            mbuffer->unBindHalfDuplexOut(process_id, *iter);

        mbuffer->unBindHalfDuplexOut(process_id, MainBuffer::DEFAULT_ID);
    }

    return recordStatus;
}

std::string Conference::getConfID() const {
    return id_;
}

