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
#include "callstreamsmanager.h"
#include "sip/sipcall.h"

namespace jami {

CallStreamsManager::CallStreamsManager() {}

void
CallStreamsManager::setLayout(int layout)
{
    if (layout < 0 || layout > 2) {
        JAMI_ERR("Unknown layout %u", layout);
        return;
    }
    currentLayout_ = static_cast<Layout>(layout);
    if (currentLayout_ == Layout::GRID)
        activeStream_.clear();
    updateInfo();
}

void
CallStreamsManager::setVoiceActivity(const std::string& streamId, bool newState)
{
    auto& info = streamsInfo_[streamId];
    auto updated = info.voiceActivity == newState;
    info.voiceActivity = newState;

    if (updated)
        updateInfo();
}

void
CallStreamsManager::setActiveStream(const std::string& streamId, bool newState)
{
    if (!activeStream_.empty() && newState) {
        auto& info = streamsInfo_[activeStream_];
        info.active = false;
    }
    auto& info = streamsInfo_[streamId];
    bool updated = info.active == newState;
    info.active = newState;
    activeStream_ = streamId;

    if (updated)
        updateInfo();
}

void
CallStreamsManager::muteStream(const std::string& streamId, bool newState)
{
    auto& info = streamsInfo_[streamId];
    bool updated = info.audioModeratorMuted;
    info.audioModeratorMuted = newState;

    if (updated)
        updateInfo();
}

bool
CallStreamsManager::isMuted(const std::string& streamId)
{
    auto it = streamsInfo_.find(streamId);
    if (it == streamsInfo_.end())
        return it->second.audioModeratorMuted;
    return false;
}

void
CallStreamsManager::detachStream(const std::string& streamId)
{
    streamsInfo_.erase(streamId);
    updateInfo();
}

void
CallStreamsManager::bindCall(const std::shared_ptr<SIPCall>& call)
{
    JAMI_ERROR("@@@ BIND {}", call->getCallId());
    auto uri = call->getRemoteUri();
    auto deviceId = call->getRemoteDeviceId();
    auto& sInfo = streamsInfo_[sip_utils::streamId(call->getCallId(), sip_utils::DEFAULT_AUDIO_STREAMID)];
    sInfo.videoMuted = not MediaAttribute::hasMediaType(call->getMediaAttributeList(), MediaType::MEDIA_VIDEO);
    sInfo.audioLocalMuted = call->isPeerMuted();
    // TODO only in deviceInfo
    sInfo.uri = uri;
    sInfo.device = deviceId;
    auto& dInfo = devicesInfo_[std::make_pair(uri, deviceId)];
    dInfo.recording = call->isRecording();
    // TODO use only key
    dInfo.uri = uri;
    dInfo.device = deviceId;
    updateInfo();
}

void
CallStreamsManager::setHandRaised(const std::string& uri, const std::string& deviceId, bool newState)
{
    JAMI_ERROR("@@@@ UPDATE HAND {} {} {} ", newState, uri, deviceId);
    // TOOD check if found!
    // TODO: group deviceInfo into streamInfo
    auto key = std::make_pair(uri, deviceId);
    auto it = devicesInfo_.find(key);
    if (it == devicesInfo_.end()) {
        if (it->second.handRaised != newState) {
            it->second.handRaised = newState;
            JAMI_ERROR("@@@@ UPDATED");
            updateInfo();
        }
    }
}

void
CallStreamsManager::setRecording(const std::string& uri, const std::string& deviceId, bool newState)
{
    auto& info = devicesInfo_[std::make_pair(uri, deviceId)];
    bool updated = info.recording == newState;
    info.recording = newState;

    if (updated)
        updateInfo();
}

void
CallStreamsManager::setModerator(const std::string& uri, const std::string& deviceId, bool newState)
{
    auto& info = devicesInfo_[std::make_pair(uri, deviceId)];
    bool updated = info.isModerator == newState;
    info.isModerator = newState;

    if (updated)
        updateInfo();
}

void
CallStreamsManager::removeCall(const std::string& uri, const std::string& deviceId)
{
    auto key = std::make_pair(uri, deviceId);
    auto itK = devicesInfo_.find(key);
    if (itK == devicesInfo_.end())
        return;
    for (auto it = streamsInfo_.begin(); it != streamsInfo_.end();) {
        if (it->second.uri == uri && it->second.device == deviceId) {
            if (it->second.active)
                activeStream_.clear();
            it = streamsInfo_.erase(it);
        }
        else
            ++it;
    }
    devicesInfo_.erase(itK);
    updateInfo();
}

void
CallStreamsManager::updateInfo()
{
    // TODO update streamsInfo_ with devicesInfo_
    // Update upper layers
    if (onInfoUpdated_) {
        JAMI_ERROR("@@@ GO UPDATE");
        onInfoUpdated_(streamsInfo_);
    }
}

} // namespace jami
