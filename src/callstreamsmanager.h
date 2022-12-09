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

#include <map>
#include <functional>
#include <string>
#include <vector>

#include <json/json.h>

namespace jami {

class SIPCall;

enum class Layout { GRID, ONE_BIG_WITH_SMALL, ONE_BIG };

// info for a stream
struct StreamInfo
{
    std::string uri;
    std::string device;
    std::string sinkId; // stream ID
    bool active {false};
    int x {0};
    int y {0};
    int w {0};
    int h {0};
    bool videoMuted {false};
    bool audioLocalMuted {false};
    bool audioModeratorMuted {false};
    bool isModerator {false};
    bool handRaised {false};
    bool voiceActivity {false};
    bool recording {false};

    void fromJson(const Json::Value& v)
    {
        uri = v["uri"].asString();
        device = v["device"].asString();
        sinkId = v["sinkId"].asString();
        active = v["active"].asBool();
        x = v["x"].asInt();
        y = v["y"].asInt();
        w = v["w"].asInt();
        h = v["h"].asInt();
        videoMuted = v["videoMuted"].asBool();
        audioLocalMuted = v["audioLocalMuted"].asBool();
        audioModeratorMuted = v["audioModeratorMuted"].asBool();
        isModerator = v["isModerator"].asBool();
        handRaised = v["handRaised"].asBool();
        voiceActivity = v["voiceActivity"].asBool();
        recording = v["recording"].asBool();
    }

    Json::Value toJson() const
    {
        Json::Value val;
        val["uri"] = uri;
        val["device"] = device;
        val["sinkId"] = sinkId;
        val["active"] = active;
        val["x"] = x;
        val["y"] = y;
        val["w"] = w;
        val["h"] = h;
        val["videoMuted"] = videoMuted;
        val["audioLocalMuted"] = audioLocalMuted;
        val["audioModeratorMuted"] = audioModeratorMuted;
        val["isModerator"] = isModerator;
        val["handRaised"] = handRaised;
        val["voiceActivity"] = voiceActivity;
        val["recording"] = recording;
        return val;
    }

    std::map<std::string, std::string> toMap() const
    {
        return {{"uri", uri},
                {"device", device},
                {"sinkId", sinkId},
                {"active", active ? "true" : "false"},
                {"x", std::to_string(x)},
                {"y", std::to_string(y)},
                {"w", std::to_string(w)},
                {"h", std::to_string(h)},
                {"videoMuted", videoMuted ? "true" : "false"},
                {"audioLocalMuted", audioLocalMuted ? "true" : "false"},
                {"audioModeratorMuted", audioModeratorMuted ? "true" : "false"},
                {"isModerator", isModerator ? "true" : "false"},
                {"handRaised", handRaised ? "true" : "false"},
                {"voiceActivity", voiceActivity ? "true" : "false"},
                {"recording", recording ? "true" : "false"}};
    }

    friend bool operator==(const StreamInfo& p1, const StreamInfo& p2)
    {
        return p1.uri == p2.uri and p1.device == p2.device and p1.sinkId == p2.sinkId
               and p1.active == p2.active and p1.x == p2.x and p1.y == p2.y and p1.w == p2.w
               and p1.h == p2.h and p1.videoMuted == p2.videoMuted
               and p1.audioLocalMuted == p2.audioLocalMuted
               and p1.audioModeratorMuted == p2.audioModeratorMuted
               and p1.isModerator == p2.isModerator and p1.handRaised == p2.handRaised
               and p1.voiceActivity == p2.voiceActivity and p1.recording == p2.recording;
    }

    friend bool operator!=(const StreamInfo& p1, const StreamInfo& p2)
    {
        return !(p1 == p2);
    }
};

struct DeviceInfo
{
    std::string uri;
    std::string device;
    bool handRaised {false};
    bool recording {false};
    bool isModerator {false};
};

using StreamInfoMap = std::map<std::string, StreamInfo> /* streamId, streamInfo */;
using DeviceInfoMap = std::map<std::pair<std::string, std::string>, DeviceInfo> /* {uri, deviceId}, DeviceInfo */;

class CallStreamsManager
{
public:
    CallStreamsManager();

    virtual ~CallStreamsManager() = default;

    // General
    virtual void setLayout(int layout);
    virtual Layout getLayout() const { return currentLayout_; }

    virtual void setOnInfoUpdated(std::function<void(const StreamInfoMap& info)> onInfoUpdated) {
        onInfoUpdated_ = std::move(onInfoUpdated);
    }

    // Per streams
    virtual void setVoiceActivity(const std::string& streamId, bool newState);
    virtual void muteStream(const std::string& streamId, bool newState);
    virtual bool isMuted(const std::string& streamId);
    virtual void setActiveStream(const std::string& streamId, bool newState);
    virtual void detachStream(const std::string& streamId);

    // Per calls
    virtual void bindCall(const std::shared_ptr<SIPCall>& call);
    virtual void setHandRaised(const std::string& uri, const std::string& deviceId, bool newState);
    virtual void setRecording(const std::string& uri, const std::string& deviceId, bool newState);
    virtual void setModerator(const std::string& uri, const std::string& deviceId, bool newState);
    virtual void removeCall(const std::string& uri, const std::string& deviceId);

protected:
    /**
     * Triggers onInfoUpdated
     */
    void updateInfo();

    std::function<void(const StreamInfoMap& info)> onInfoUpdated_;
    StreamInfoMap streamsInfo_; // TODO lock
    DeviceInfoMap devicesInfo_;

    std::string activeStream_ {};
    Layout currentLayout_ {Layout::GRID};
};

} // namespace jami
