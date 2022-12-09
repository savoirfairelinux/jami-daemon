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
    auto& info = streamsInfo_[streamId];
    auto updated = info.voiceActivity == newState;
    info.voiceActivity = newState;

    if (updated)
        updateInfo();
}

void
CallStreamsManager::setActiveStream(const std::string& streamId)
{
    auto& info = streamsInfo_[streamId];
    bool updated = info.active;
    info.active = true;

    if (updated)
        updateInfo();
}

void
CallStreamsManager::muteStream(const std::string& streamId, bool newState)
{
    auto& info = streamsInfo_[streamId];
    bool updated = info.active;
    info.active = true;

    if (updated)
        updateInfo();
}

void
CallStreamsManager::detachStream(const std::string& streamId)
{
    streamsInfo_.erase(streamId);
    updateInfo();
}

void
CallStreamsManager::setHandRaised(const std::string& uri, const std::string& deviceId, bool newState)
{
    // TOOD check if found!
    auto& info = devicesInfo_[{uri, deviceId}];
    bool updated = info.handRaised == newState;
    info.handRaised = newState;

    if (updated)
        updateInfo();
}

void
CallStreamsManager::setRecording(const std::string& uri, const std::string& deviceId, bool newState)
{
    auto& info = devicesInfo_[{uri, deviceId}];
    bool updated = info.recording == newState;
    info.recording = newState;

    if (updated)
        updateInfo();
}

void
CallStreamsManager::removeCall(const std::string& uri, const std::string& deviceId)
{
    auto& info = devicesInfo_.erase({uri, deviceId});
    for (auto it = streamsInfo_.begin(); it != streamsInfo_.end();) {
        if (it->second.uri == uri && it->second.device == deviceId)
            it = streamsInfo_.erase(it);
        else
            ++it;
    }
    updateInfo();
}

void
CallStreamsManager::updateInfo()
{
    // TODO update streamsInfo_ with devicesInfo_
    // Update upper layers
    if (onInfoUpdated_)
        onInfoUpdated_(streamsInfo_);
}

} // namespace jami
