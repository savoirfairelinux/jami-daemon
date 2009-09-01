/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
 *  Author: Alexandre Savard  <alexandre.savard@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef CONFERENCE_H
#define CONFERENCE_H

#include <set>

#include "call.h"

// class ManagerImpl;
// class Call;

typedef std::string ConfID;

typedef std::set<CallID> ParticipantSet;

class Conference{

    public:

        enum ConferenceState {Active, Hold};

        Conference(ConfID confID);

        ~Conference();

	std::string getConfID() { return _id; }

	int getState();

	void setState(ConferenceState state);

	std::string getStateStr();

	int getNbParticipants() { return _nbParticipant; }

	void add(CallID participant_id);

	void remove(CallID participant_id);

	void bindParticipant(CallID participant_id);

	CallID getLastParticipant();

    private:  

        /** Unique ID of the conference */
        CallID _id;

	ConferenceState _confState;

        ParticipantSet _participants;

        int _nbParticipant;
};

#endif
