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
class MediaAttribute;

enum class Layout { GRID, ONE_BIG_WITH_SMALL, ONE_BIG };

// info for a stream
struct StreamInfo
{
    bool active {false};
    int x {0};
    int y {0};
    int w {0};
    int h {0};
    bool videoMuted {false};
    bool audioLocalMuted {false};
    bool audioModeratorMuted {false};
    bool voiceActivity {false};

    friend bool operator==(const StreamInfo& p1, const StreamInfo& p2)
    {
        return p1.active == p2.active and p1.x == p2.x and p1.y == p2.y and p1.w == p2.w
               and p1.h == p2.h and p1.videoMuted == p2.videoMuted
               and p1.audioLocalMuted == p2.audioLocalMuted
               and p1.audioModeratorMuted == p2.audioModeratorMuted
               and p1.voiceActivity == p2.voiceActivity;
    }

    friend bool operator!=(const StreamInfo& p1, const StreamInfo& p2)
    {
        return !(p1 == p2);
    }
};

struct CallInfo
{
    bool handRaised {false};
    bool recording {false};
    bool isModerator {false};

    friend bool operator==(const CallInfo& p1, const CallInfo& p2)
    {
        return p1.handRaised == p2.handRaised and p1.recording == p2.recording and p1.isModerator == p2.isModerator
                and p1.streams.size() == p2.streams.size() and std::equal(p1.streams.begin(), p1.streams.end(), p2.streams.begin());
    }

    friend bool operator!=(const CallInfo& p1, const CallInfo& p2)
    {
        return !(p1 == p2);
    }

    std::map<std::string, StreamInfo> streams;
};

using CallInfoMap = std::map<std::pair<std::string, std::string>, CallInfo> /* (uri, device), CallInfo */;

class CallStreamsManager
{
public:
    CallStreamsManager();

    virtual ~CallStreamsManager() = default;

    // General
    virtual void setLayout(int layout);
    virtual Layout getLayout() const { return currentLayout_; }

    virtual void setOnInfoUpdated(std::function<void(const CallInfoMap& info)> onInfoUpdated) {
        onInfoUpdated_ = std::move(onInfoUpdated);
    }

    void setAccountInfo(const std::string& uri, const std::string& deviceId) {
        uri_ = uri;
        deviceId_ = deviceId;
    }

    // Per streams
    virtual void setStreams(const std::string& uri, const std::string& device, const std::vector<MediaAttribute>& streams);
    virtual void setVoiceActivity(const std::string& uri, const std::string& deviceId, const std::string& streamId, bool newState);
    virtual void muteStream(const std::string& uri, const std::string& deviceId, const std::string& streamId, bool newState);
    virtual bool isMuted(const std::string& uri, const std::string& deviceId, const std::string& streamId);
    virtual void setActiveStream(const std::string& uri, const std::string& deviceId,const std::string& streamId, bool newState);

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

    /**
     * For a mixer, we do not want to show audio_0 if video_0 is there
     * This method will remove xxxx_audio_y if xxxx_video_y is present
     */
    void replaceAudioStream(const std::string& uri, const std::string& deviceId, const std::string& videoStream);

    std::function<void(const CallInfoMap& info)> onInfoUpdated_;
    CallInfoMap callInfo_;

    std::pair<std::pair<std::string, std::string>, std::string> activeStream_ {};
    Layout currentLayout_ {Layout::GRID};

    std::string uri_;
    std::string deviceId_;
};

} // namespace jami
