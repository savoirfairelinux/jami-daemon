/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

int Conference::count = 0;

Conference::Conference()
{
    _nbParticipant = 0;

    ++count;

    std::string conf ("conf_");

    // convert count into string
    std::string s;
    std::stringstream out;
    out << count;
    s = out.str();

    _nbParticipant = 0;
    _id = conf.append (s);



}


Conference::~Conference()
{



}


int Conference::getState()
{
    return _confState;
}


void Conference::setState (ConferenceState state)
{
    _confState = state;
}


void Conference::add (CallID participant_id)
{

    _debug ("Conference:: add participant %s", participant_id.c_str());

    _participants.insert (participant_id);

    _nbParticipant++;
}


void Conference::remove (CallID participant_id)
{


    _debug ("Conference::remove participant %s", participant_id.c_str());

    _participants.erase (participant_id);

    _nbParticipant--;

}

void Conference::bindParticipant (CallID participant_id)
{

    if (_nbParticipant >= 1) {
        ParticipantSet::iterator iter = _participants.begin();

        while (iter != _participants.end()) {

            if (participant_id != (*iter)) {
                Manager::instance().getAudioDriver()->getMainBuffer()->bindCallID (participant_id, *iter);
            }

            iter++;
        }

    }


    Manager::instance().getAudioDriver()->getMainBuffer()->bindCallID (participant_id);

}


std::string Conference::getStateStr()
{

    std::string state_str;

    switch (_confState) {

        case Active_Atached:
            state_str = "ACTIVE_ATACHED";
            break;

        case Active_Detached:
            state_str = "ACTIVE_DETACHED";
            break;

        case Hold:
            state_str = "HOLD";
            break;

        default:
            break;
    }

    return state_str;
}


ParticipantSet Conference::getParticipantList()
{
    return _participants;
}

