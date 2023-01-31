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

#include "media_const.h"
#include "sip/sipcall.h"

namespace jami {

CallStreamsManager::CallStreamsManager() {}

void
CallStreamsManager::setLayout(int layout)
{
    if (layout < 0 || layout > 2) {
        JAMI_ERROR("Unknown layout {}", layout);
        return;
    }
    currentLayout_ = static_cast<Layout>(layout);
    if (currentLayout_ == Layout::GRID) {
        if (!activeStream_.second.empty())
            callInfo_[activeStream_.first].streams[activeStream_.second].active = false;
        activeStream_ = {};
    }
    updateInfo();
}

void
CallStreamsManager::setStreams(const std::string& uri, const std::string& device, const std::vector<MediaAttribute>& streams)
{
    // TODO only video if audio
    auto key = std::make_pair(uri, device);
    auto& callInfo = callInfo_[key];
    callInfo.streams.clear();
    for (const auto& stream : streams) {
        auto streamId = sip_utils::streamId("", stream.label_);
        if (streamId.find("video") != std::string::npos) {
            // Video stream, we can replace audio
            replaceAudioStream(uri, device, streamId);
            auto& sInfo = callInfo.streams[streamId];
            sInfo.videoMuted = stream.muted_;
        } else {
            // Audio stream, nothing to do if video
            std::string videoStream = streamId;
            string_replace(videoStream, "audio", "video");
            if (callInfo.streams.find(videoStream) == callInfo.streams.end()) {
                auto& sInfo = callInfo.streams[streamId];
                sInfo.videoMuted = stream.muted_;
            }
        }
    }
    updateInfo();
}

void
CallStreamsManager::setVoiceActivity(const std::string& uri, const std::string& deviceId, const std::string& streamId, bool newState)
{
    auto key = std::make_pair(uri, deviceId);
    auto it = callInfo_.find(key);
    if (it != callInfo_.end()) {
        auto itStream = it->second.streams.find(streamId);
        if (itStream != it->second.streams.end()
            && itStream->second.voiceActivity != newState) {
            itStream->second.voiceActivity = newState;
            updateInfo();
        }
    }
}

void
CallStreamsManager::muteStream(const std::string& uri, const std::string& deviceId, const std::string& streamId, bool newState)
{
    auto key = std::make_pair(uri, deviceId);
    auto it = callInfo_.find(key);
    if (it != callInfo_.end()) {
        auto itStream = it->second.streams.find(streamId);
        if (itStream != it->second.streams.end()
            && itStream->second.audioModeratorMuted != newState) {
            itStream->second.audioModeratorMuted = newState;
            updateInfo();
        }
    }
}

bool
CallStreamsManager::isMuted(const std::string& uri, const std::string& deviceId, const std::string& streamId)
{
    auto key = std::make_pair(uri, deviceId);
    auto it = callInfo_.find(key);
    if (it != callInfo_.end()) {
        auto itStream = it->second.streams.find(streamId);
        if (itStream != it->second.streams.end())
            return itStream->second.audioModeratorMuted;
    }
    return false;
}

void
CallStreamsManager::setActiveStream(const std::string& uri, const std::string& deviceId, const std::string& streamId, bool newState)
{
    if (!activeStream_.second.empty() && newState)
        callInfo_[activeStream_.first].streams[activeStream_.second].active = false;

    auto key = std::make_pair(uri, deviceId);
    auto it = callInfo_.find(key);
    if (it != callInfo_.end()) {
        auto itStream = it->second.streams.find(streamId);
        if (itStream != it->second.streams.end()
            && itStream->second.active != newState) {
            itStream->second.active = newState;
            activeStream_ = std::make_pair(key, streamId);
            updateInfo();
        }
    }
}

void
CallStreamsManager::bindCall(const std::shared_ptr<SIPCall>& call)
{
    auto callId = call->getCallId();
    auto uri = call->getRemoteUri();
    auto deviceId = call->getRemoteDeviceId();
    auto key = std::make_pair(uri, deviceId);
    auto& callInfo = callInfo_[key];
    if (!callInfo.streams.empty()) {
        // Already binded
        return;
    }
    auto streamId = sip_utils::streamId(callId, sip_utils::DEFAULT_AUDIO_STREAMID);
    StreamInfo sInfo;
    sInfo.videoMuted = not MediaAttribute::hasMediaType(call->getMediaAttributeList(), MediaType::MEDIA_VIDEO);
    sInfo.audioLocalMuted = call->isPeerMuted();
    callInfo.streams[streamId] = std::move(sInfo);
    callInfo.recording = call->isRecording();
    updateInfo();
}

void
CallStreamsManager::setHandRaised(const std::string& uri, const std::string& deviceId, bool newState)
{
    JAMI_ERROR("@@@@ UPDATE HAND {} {} {} ", uri, deviceId, newState);
    // TOOD check if found!
    auto key = std::make_pair(uri, deviceId);
    auto it = callInfo_.find(key);
    if (it != callInfo_.end()) {
        if (it->second.handRaised != newState) {
            it->second.handRaised = newState;
            updateInfo();
        }
    }
}

void
CallStreamsManager::setRecording(const std::string& uri, const std::string& deviceId, bool newState)
{
    auto key = std::make_pair(uri, deviceId);
    auto it = callInfo_.find(key);
    if (it != callInfo_.end()) {
        if (it->second.recording != newState) {
            it->second.recording = newState;
            updateInfo();
        }
    }
}

void
CallStreamsManager::setModerator(const std::string& uri, const std::string& deviceId, bool newState)
{
    auto key = std::make_pair(uri, deviceId);
    auto it = callInfo_.find(key);
    if (it != callInfo_.end()) {
        if (it->second.isModerator != newState) {
            it->second.isModerator = newState;
            updateInfo();
        }
    }
}

void
CallStreamsManager::removeCall(const std::string& uri, const std::string& deviceId)
{
    auto key = std::make_pair(uri, deviceId);
    auto itCall = callInfo_.find(key);
    if (itCall == callInfo_.end())
        return;
    callInfo_.erase(itCall);
    updateInfo();
}

void
CallStreamsManager::updateInfo()
{
    // Update upper layers
    if (onInfoUpdated_)
        onInfoUpdated_(callInfo_);
}

void
CallStreamsManager::replaceAudioStream(const std::string& uri, const std::string& deviceId, const std::string& videoStream)
{
    auto key = std::make_pair(uri, deviceId);
    auto itCall = callInfo_.find(key);
    if (itCall == callInfo_.end()) {
        auto& ci = callInfo_[key];
        ci.streams[videoStream] = {};
        return;
    }
    std::string audioStream = videoStream;
    string_replace(audioStream, "video", "audio");
    itCall->second.streams[videoStream] = itCall->second.streams[audioStream];
    itCall->second.streams.erase(audioStream);
}


} // namespace jami
