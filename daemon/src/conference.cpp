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
	: _id (Manager::instance().getNewCallID())
	, _confState (ACTIVE_ATTACHED)
{
    Recordable::initRecFileName (_id);
}

int Conference::getState() const
{
    return _confState;
}

void Conference::setState (ConferenceState state)
{
    _confState = state;
}

void Conference::add(const std::string &participant_id)
{
    _participants.insert (participant_id);
}

void Conference::remove(const std::string &participant_id)
{
    _participants.erase(participant_id);
}

void Conference::bindParticipant(const std::string &participant_id)
{
	for (ParticipantSet::iterator iter = _participants.begin();
            iter != _participants.end(); ++iter)
        if (participant_id != *iter)
            Manager::instance().getMainBuffer()->bindCallID(participant_id, *iter);

    Manager::instance().getMainBuffer()->bindCallID(participant_id);
}

std::string Conference::getStateStr()
{
    switch (_confState) {
        case ACTIVE_ATTACHED:		return "ACTIVE_ATTACHED";
        case ACTIVE_DETACHED:		return "ACTIVE_DETACHED";
        case ACTIVE_ATTACHED_REC:	return "ACTIVE_ATTACHED_REC";
        case ACTIVE_DETACHED_REC:	return "ACTIVE_DETACHED_REC";
        case HOLD:					return "HOLD";
        case HOLD_REC:				return "HOLD_REC";
        default:					return "";
    }
}

ParticipantSet Conference::getParticipantList() const
{
    return _participants;
}

bool Conference::setRecording()
{
    bool recordStatus = Recordable::recAudio.isRecording();

    Recordable::recAudio.setRecording();
    MainBuffer *mbuffer = Manager::instance().getMainBuffer();

    ParticipantSet::iterator iter;
    std::string process_id = Recordable::recorder.getRecorderID();

    // start recording
    if (!recordStatus) {
        for (iter = _participants.begin(); iter != _participants.end(); ++iter)
            mbuffer->bindHalfDuplexOut (process_id, *iter);

        mbuffer->bindHalfDuplexOut (process_id);

        Recordable::recorder.start();
    } else {
        for (iter = _participants.begin(); iter != _participants.end(); ++iter)
            mbuffer->unBindHalfDuplexOut (process_id, *iter);

        mbuffer->unBindHalfDuplexOut (process_id);
    }

    return recordStatus;
}
