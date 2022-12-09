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

#include "client/videomanager.h"
#include "manager.h"
#include "media_const.h"
#include "media_stream.h"
#include "sip/sipcall.h"

namespace jami {

static constexpr const auto MIXER_FRAMERATE = 30;

CallStreamsManager::CallStreamsManager()
    : VideoGenerator::VideoGenerator()
{}

void
CallStreamsManager::setLayout(int layout)
{
    if (layout < 0 || layout > 2) {
        JAMI_ERROR("Unknown layout {}", layout);
        return;
    }
    currentLayout_ = static_cast<Layout>(layout);
    if (currentLayout_ == Layout::GRID) {
        if (!activeStream_.second.empty()) {
            std::lock_guard<std::mutex> lk(callInfoMtx_);
            callInfo_[activeStream_.first].streams[activeStream_.second].active = false;
        }
        activeStream_ = {};
    }
    updateInfo();
}

void
CallStreamsManager::setStreams(const std::string& uri, const std::string& device, const std::vector<MediaAttribute>& streams)
{
    auto key = std::make_pair(uri, device);
    std::unique_lock<std::mutex> lk(callInfoMtx_);
    auto& callInfo = callInfo_[key];
    callInfo.streams.clear();

    // Do not stop video inputs that are already there
    // But only detach it to get new index
    decltype(localInputs_) newInputs;

    for (const auto& stream : streams) {
        auto streamId = sip_utils::streamId("", stream.label_);
        if (streamId.find("video") != std::string::npos) {
            // Video stream, we can replace audio
            replaceAudioStream(uri, device, streamId);
            auto& sInfo = callInfo.streams[streamId];
            sInfo.videoMuted = stream.muted_;
#ifdef ENABLE_VIDEO
            auto videoInput = getVideoInput(stream.sourceUri_); // TODO enable_video
            // Note, video can be a previously stopped device (eg. restart a screen sharing)
            // in this case, the videoInput will be found and must be restarted
            videoInput->restart();
            auto onlyDetach = false;
            auto it = std::find(localInputs_.cbegin(), localInputs_.cend(), videoInput);
            onlyDetach = it != localInputs_.cend();
            newInputs.emplace_back(videoInput);
            if (onlyDetach) {
                videoInput->detach(this);
                localInputs_.erase(it);
            }
#endif
        } else {
            // Audio stream, nothing to do if video
            std::string videoStream = streamId;
            string_replace(videoStream, "audio", "video");
            if (callInfo.streams.find(videoStream) == callInfo.streams.end()) {
                auto& sInfo = callInfo.streams[streamId];
                sInfo.audioLocalMuted = stream.muted_;
                sInfo.videoMuted = true;
            }
        }
    }
    updateInfo();
    lk.unlock();

#ifdef ENABLE_VIDEO
    // Stop other video inputs
    stopInputs();
    localInputs_ = std::move(newInputs);

    // Re-attach videoInput to mixer
    // TODO use ID from streams()
    for (auto i = 0u; i != localInputs_.size(); ++i)
        attachVideo(localInputs_[i].get(), "", sip_utils::streamId("", fmt::format("video_{}", i)));
#endif
}

void
CallStreamsManager::setVoiceActivity(const std::string& uri, const std::string& deviceId, const std::string& streamId, bool newState)
{
    auto key = std::make_pair(uri, deviceId);
    std::lock_guard<std::mutex> lk(callInfoMtx_);
    auto it = callInfo_.find(key);
    if (it != callInfo_.end()) {
        auto itStream = it->second.streams.find(streamId);
        if (itStream == it->second.streams.end()) {
            auto sid = streamId;
            string_replace(sid, "audio", "video");
            itStream = it->second.streams.find(sid);
        }
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
    std::lock_guard<std::mutex> lk(callInfoMtx_);
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
        }
    }
}

bool
CallStreamsManager::isMuted(const std::string& uri, const std::string& deviceId, const std::string& streamId, bool onlyLocal) const
{
    auto key = std::make_pair(uri, deviceId);
    std::lock_guard<std::mutex> lk(callInfoMtx_);
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
    std::lock_guard<std::mutex> lk(callInfoMtx_);
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
    std::lock_guard<std::mutex> lk(callInfoMtx_);
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
    std::lock_guard<std::mutex> lk(callInfoMtx_);
    auto callId = call->getCallId();
    callIds_.insert(callId);
    auto uri = call->getRemoteUri();
    auto deviceId = call->getRemoteDeviceId();
    if (uri.empty() || deviceId.empty()) // Usefull for now because of call-swarm (conference with one un-connected call)
        return;
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
    std::lock_guard<std::mutex> lk(callInfoMtx_);
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
    std::lock_guard<std::mutex> lk(callInfoMtx_);
    auto key = std::make_pair(uri, deviceId);
    auto it = callInfo_.find(key);
    if (it != callInfo_.end()) {
        if (it->second.recording != newState) {
            it->second.recording = newState;
            updateInfo();
        }
    }
}

MediaStream
CallStreamsManager::getStream(const std::string& name) const
{
    MediaStream ms;
    ms.name = name;
    ms.format = format_;
    ms.isVideo = true;
    ms.height = height_;
    ms.width = width_;
    ms.frameRate = {MIXER_FRAMERATE, 1};
    ms.timeBase = {1, MIXER_FRAMERATE};
    ms.firstTimestamp = lastTimestamp_;

    return ms;
}

void
CallStreamsManager::setModerator(const std::string& uri, bool newState)
{
    std::lock_guard<std::mutex> lk(callInfoMtx_);
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
    std::lock_guard<std::mutex> lk(callInfoMtx_);
    for (auto& [key, value]: callInfo_)
        if (key.first == uri)
            return value.isModerator;
    return false;
}

void
CallStreamsManager::removeCall(const std::shared_ptr<SIPCall>& call)
{
    std::lock_guard<std::mutex> lk(callInfoMtx_);
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
CallStreamsManager::replaceVideoStream(const std::string& uri, const std::string& deviceId, const std::string& audioStream)
{
    auto key = std::make_pair(uri, deviceId);
    auto itCall = callInfo_.find(key);
    if (itCall == callInfo_.end()) {
        auto& ci = callInfo_[key];
        ci.streams[audioStream] = {};
        return;
    }
    std::string videoStream = audioStream;
    string_replace(videoStream, "audio", "video");
    auto itVideo = itCall->second.streams.find(videoStream);
    if (itVideo != itCall->second.streams.end()) {
        itCall->second.streams[audioStream] = itVideo->second;
        itCall->second.streams.erase(itVideo);
    } else {
        itCall->second.streams[audioStream] = {};
    }
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
    auto itAudio = itCall->second.streams.find(audioStream);
    if (itAudio != itCall->second.streams.end()) {
        itCall->second.streams[videoStream] = itAudio->second;
        itCall->second.streams.erase(itAudio);
    } else {
        itCall->second.streams[videoStream] = {};
    }
}

void
CallStreamsManager::attachVideo(Observable<std::shared_ptr<MediaFrame>>* frame,
                        const std::string& callId,
                        const std::string& streamId)
{
    if (!frame)
        return;
    JAMI_DEBUG("Attaching video with streamId {:s}", streamId);
    if (auto call = std::dynamic_pointer_cast<SIPCall>(Manager::instance().getCallFromCallID(callId)))
        replaceAudioStream(call->getRemoteUri(), call->getRemoteDeviceId(), streamId);
    {
        std::lock_guard<std::mutex> lk(callInfoMtx_);
        videoToStreamId_[frame] = StreamId {callId, streamId};
        streamIdToSource_[streamId] = frame;
    }
    frame->attach(this);
    updateInfo();
}

void
CallStreamsManager::detachVideo(Observable<std::shared_ptr<MediaFrame>>* frame)
{
    if (!frame)
        return;
    bool detach = false;
    std::unique_lock<std::mutex> lk(callInfoMtx_);
    auto it = videoToStreamId_.find(frame);
    if (it != videoToStreamId_.end()) {
        JAMI_DEBUG("Detaching video of call {:s}", it->second.callId);
        detach = true;
        // Handle the case where the current shown source leave the conference
        if (auto call = std::dynamic_pointer_cast<SIPCall>(Manager::instance().getCallFromCallID(it->second.callId))) {
            auto streamId = it->second.streamId;
            string_replace(streamId, "video", "audio");
            replaceVideoStream(call->getRemoteUri(), call->getRemoteDeviceId(), streamId);
            streamIdToSource_.erase(it->second.streamId);
        }
        videoToStreamId_.erase(it);
    }
    lk.unlock();
    if (detach) {
        frame->detach(this);
        updateInfo();
    }
}

#ifdef ENABLE_VIDEO
void
CallStreamsManager::stopInput(const std::shared_ptr<VideoFrameActiveWriter>& input)
{
    // Detach videoInputs from mixer
    input->detach(this);
#if !VIDEO_CLIENT_INPUT
    // Stop old VideoInput
    if (auto oldInput = std::dynamic_pointer_cast<video::VideoInput>(input))
        oldInput->stopInput();
#endif
}

void
CallStreamsManager::stopInputs()
{
    for (auto& input : localInputs_)
        stopInput(input);
    localInputs_.clear();
}
#endif

std::vector<std::map<std::string, std::string>>
ConfInfo::toVectorMapStringString() const
{
    std::vector<std::map<std::string, std::string>> res;
    for (const auto& [key, value] : callInfo_) {
        auto& [uri, device] = key;
        std::map<std::string, std::string> baseValue;
        baseValue["uri"] = string_remove_suffix(uri, '@');
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
        baseValue["uri"] = std::string(string_remove_suffix(uri, '@'));
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
