/*
 *  Copyright (C) 2004-2018 Savoir-faire Linux Inc.
 *
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */
#ifndef CONFERENCE_H
#define CONFERENCE_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <set>
#include <string>
#include <memory>

#include "recordable.h"

namespace ring {

#ifdef RING_VIDEO
namespace video {
class VideoMixer;
}
#endif

typedef std::set<std::string> ParticipantSet;

class Conference : public Recordable {
    public:
        enum ConferenceState {ACTIVE_ATTACHED, ACTIVE_DETACHED, ACTIVE_ATTACHED_REC, ACTIVE_DETACHED_REC, HOLD, HOLD_REC};

        /**
         * Constructor for this class, increment static counter
         */
        Conference();

        /**
         * Destructor for this class, decrement static counter
         */
        ~Conference();

        /**
         * Return the conference id
         */
        std::string getConfID() const;

        /**
         * Return the current conference state
         */
        ConferenceState getState() const;

        /**
         * Set conference state
         */
        void setState(ConferenceState state);

        /**
         * Return a string description of the conference state
         */
        std::string getStateStr() const;

        /**
         * Add a new participant to the conference
         */
        void add(const std::string &participant_id);

        /**
         * Remove a participant from the conference
         */
        void remove(const std::string &participant_id);

        /**
         * Bind a participant to the conference
         */
        void bindParticipant(const std::string &participant_id);

        /**
         * Get the participant list for this conference
         */
        ParticipantSet getParticipantList() const;

        /**
         * Get the display names or peer numbers for this conference
         */
        std::vector<std::string>
        getDisplayNames() const;

        /**
         * Start/stop recording toggle
         */
        virtual bool toggleRecording();

#ifdef RING_VIDEO
        std::shared_ptr<video::VideoMixer> getVideoMixer();
#endif

    private:
        std::string id_;
        ConferenceState confState_;
        ParticipantSet participants_;

#ifdef RING_VIDEO
        std::shared_ptr<video::VideoMixer> videoMixer_;
#endif
};

} // namespace ring

#endif
