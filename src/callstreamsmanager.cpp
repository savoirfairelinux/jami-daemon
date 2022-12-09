/*
 *  Copyright (C) 2022 Savoir-faire Linux Inc.
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
#pragma once

#include "callstreamsmanager.h"

namespace jami {

void
CallStreamsManager::setVoiceActivity(const std::string& streamId, const bool& newState)
{
    // TODO transform to map
    bool updated = false;
    for (auto& participant : streamsInfo_) {
        if (participant.sinkId == streamId) {
            updated = newState != participant.voiceActivity;
            participant.voiceActivity = newState;
            break;
        }
    }

    if (updated)
        updateInfo();
}

void
CallStreamsManager::setActiveStream(const std::string& streamId)
{
    // TODO transform to map
    bool updated = false;
    for (auto& participant : streamsInfo_) {
        if (participant.sinkId == streamId) {
            participant.active = true;
            break;
        }
    }

    if (updated)
        updateInfo();
}

void
CallStreamsManager::updateInfo()
{
    if (onInfoUpdated_)
        onInfoUpdated_(streamsInfo_);
}

} // namespace jami
