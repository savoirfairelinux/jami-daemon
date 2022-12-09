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
CallStreamsManager::muteStream(const std::string& uri, const std::string& deviceId, const std::string& streamId, bool newState, bool local)
{
    auto key = std::make_pair(uri, deviceId);
    auto it = callInfo_.find(key);
    auto updated = false;
    if (it != callInfo_.end()) {
        auto itStream = it->second.streams.find(streamId);
        if (itStream == it->second.streams.end()) {
            auto sid = streamId;
            string_replace(sid, "audio", "video");
            itStream = it->second.streams.find(sid);
        }
        if (itStream != it->second.streams.end()) {
            if (local) {
                if (streamId.find("audio") && itStream->second.audioLocalMuted != newState) {
                    itStream->second.audioLocalMuted = newState;
                    updated = true;
                } else if (streamId.find("video") != std::string::npos && itStream->second.videoMuted != newState) {
                    itStream->second.videoMuted = newState;
                    updated = true;
                }
            } else {
                if (streamId.find("audio") && itStream->second.audioModeratorMuted != newState) {
                    itStream->second.audioModeratorMuted = newState;
                    updated = true;
                } else {
                    // Video not supported yet
                }
            }
            if (updated)
                updateInfo();
        } else {
        }
    } else {
    }
}

bool
CallStreamsManager::isMuted(const std::string& uri, const std::string& deviceId, const std::string& streamId, bool onlyLocal) const
{
    auto key = std::make_pair(uri, deviceId);
    auto it = callInfo_.find(key);
    if (it != callInfo_.end()) {
        auto itStream = it->second.streams.find(streamId);
        if (itStream == it->second.streams.end()) {
            auto sid = streamId;
            string_replace(sid, "audio", "video");
            itStream = it->second.streams.find(sid);
        }
        if (itStream != it->second.streams.end()) {
            if (onlyLocal)
                return itStream->second.audioLocalMuted;
            return itStream->second.audioModeratorMuted || itStream->second.audioLocalMuted;
        }
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
CallStreamsManager::removeAudioStreams(const std::string& uri, const std::string& deviceId)
{
    auto key = std::make_pair(uri, deviceId);
    auto it = callInfo_.find(key);
    if (it != callInfo_.end()) {
        auto itStream = it->second.streams.begin();
        while (itStream != it->second.streams.end()) {
            if (itStream->first.find("audio") != std::string::npos)
                itStream = it->second.streams.erase(itStream);
            else
                ++itStream;
        }
    }
}

void
CallStreamsManager::bindCall(const std::shared_ptr<SIPCall>& call, bool isModerator)
{
    auto callId = call->getCallId();
    callIds_.insert(callId);
    auto uri = call->getRemoteUri();
    auto deviceId = call->getRemoteDeviceId();
    if (uri.empty() || deviceId.empty()) {
        updateInfo(); // TODO this one should not be necessary (useful for 1:1 swarm)
        // Bug: videoMuted stays at true
        return;
    }
    auto key = std::make_pair(uri, deviceId);
    auto& callInfo = callInfo_[key];
    callInfo.recording = call->isRecording();
    if (callInfo.isModerator != isModerator)
        callInfo.isModerator = isModerator;
    if (callInfo.streams.empty()) {
        auto streamId = sip_utils::streamId(callId, sip_utils::DEFAULT_AUDIO_STREAMID);
        StreamInfo sInfo;
        sInfo.videoMuted = not MediaAttribute::hasMediaType(call->getMediaAttributeList(), MediaType::MEDIA_VIDEO);
        sInfo.audioLocalMuted = call->isPeerMuted();
        callInfo.streams[streamId] = std::move(sInfo);
    }
    updateInfo();
}

void
CallStreamsManager::setHandRaised(const std::string& uri, const std::string& deviceId, bool newState)
{
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
CallStreamsManager::setModerator(const std::string& uri, bool newState)
{
    bool update = false;
    for (auto& [key, value]: callInfo_) {
        if (key.first == uri) {
            if (value.isModerator != newState) {
                update = true;
                value.isModerator = newState;
            }
        }
    }
    if (update)
        updateInfo();
}

bool
CallStreamsManager::isModerator(std::string_view uri) const
{
    for (auto& [key, value]: callInfo_)
        if (key.first == uri)
            return value.isModerator;
    return false;
}

void
CallStreamsManager::removeCall(const std::shared_ptr<SIPCall>& call)
{
    callIds_.erase(call->getCallId());
    auto key = std::make_pair(call->getRemoteUri(), call->getRemoteDeviceId());
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

std::vector<std::map<std::string, std::string>>
ConfInfo::toVectorMapStringString() const
{
    std::vector<std::map<std::string, std::string>> res;
    for (const auto& [key, value] : callInfo_) {
        auto& [uri, device] = key;
        std::map<std::string, std::string> baseValue;
        baseValue["uri"] = uri;
        baseValue["device"] = device;
        // Device information
        baseValue["handRaised"] = value.handRaised ? "true" : "false";
        baseValue["recording"] = value.recording ? "true" : "false";
        baseValue["isModerator"] = value.isModerator ? "true" : "false";
        for (const auto& [streamId, stream] : value.streams) {
            auto streamValue = baseValue;
            streamValue["sinkId"] = streamId;
            streamValue["active"] = stream.active ? "true" : "false";
            streamValue["x"] = std::to_string(stream.x);
            streamValue["y"] = std::to_string(stream.y);
            streamValue["w"] = std::to_string(stream.w);
            streamValue["h"] = std::to_string(stream.h);
            streamValue["videoMuted"] = stream.videoMuted ? "true" : "false";
            streamValue["audioLocalMuted"] = stream.audioLocalMuted ? "true" : "false";
            streamValue["audioModeratorMuted"] = stream.audioModeratorMuted ? "true" : "false";
            streamValue["voiceActivity"] = stream.voiceActivity ? "true" : "false";
            res.emplace_back(streamValue);
        }
    }
    return res;
}

std::string
ConfInfo::toString() const
{
    Json::Value val = {};
    for (const auto& [key, value] : callInfo_) {
        auto& [uri, device] = key;
        Json::Value baseValue;
        baseValue["uri"] = uri;
        baseValue["device"] = device;
        // Device information
        baseValue["handRaised"] = value.handRaised;
        baseValue["recording"] = value.recording;
        baseValue["isModerator"] = value.isModerator;
        for (const auto& [streamId, stream] : value.streams) {
            auto streamValue = baseValue;
            streamValue["sinkId"] = streamId;
            streamValue["active"] = stream.active;
            streamValue["x"] = stream.x;
            streamValue["y"] = stream.y;
            streamValue["w"] = stream.w;
            streamValue["h"] = stream.h;
            streamValue["videoMuted"] = stream.videoMuted;
            streamValue["audioLocalMuted"] = stream.audioLocalMuted;
            streamValue["audioModeratorMuted"] = stream.audioModeratorMuted;
            streamValue["voiceActivity"] = stream.voiceActivity;
            val["p"].append(streamValue);
        }
    }
    val["w"] = w;
    val["h"] = h;
    val["v"] = v;
    val["layout"] = layout;
    return Json::writeString(Json::StreamWriterBuilder {}, val);
}

void
ConfInfo::mergeJson(const Json::Value& jsonObj)
{
    for (const auto& participantInfo : jsonObj) {
        if (!participantInfo.isMember("uri") || !participantInfo.isMember("device") || !participantInfo.isMember("sinkId"))
            continue;
        auto key = std::make_pair(participantInfo["uri"].asString(), participantInfo["device"].asString());
        auto& callInfo = callInfo_[key];
        callInfo.handRaised = participantInfo["handRaised"].asBool();
        callInfo.recording = participantInfo["recording"].asBool();
        callInfo.isModerator = participantInfo["isModerator"].asBool();
        auto& streamInfo = callInfo.streams[participantInfo["sinkId"].asString()];
        streamInfo.active = participantInfo["active"].asBool();
        streamInfo.x = participantInfo["x"].asInt();
        streamInfo.y = participantInfo["y"].asInt();
        streamInfo.w = participantInfo["w"].asInt();
        streamInfo.h = participantInfo["h"].asInt();
        streamInfo.videoMuted = participantInfo["videoMuted"].asBool();
        streamInfo.audioLocalMuted = participantInfo["audioLocalMuted"].asBool();
        streamInfo.audioModeratorMuted = participantInfo["audioModeratorMuted"].asBool();
        streamInfo.voiceActivity = participantInfo["voiceActivity"].asBool();
    }
}

} // namespace jami
