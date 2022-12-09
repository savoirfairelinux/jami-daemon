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
#include <set>
#include <vector>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "video/video_base.h"

#include <json/json.h>

namespace jami {

class SIPCall;
class MediaAttribute;
class MediaStream;

enum class Layout { GRID, ONE_BIG_WITH_SMALL, ONE_BIG };

struct StreamId
{
    std::string callId;
    std::string streamId;
};

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
struct ConfInfo
{
    int h {0};
    int w {0};
    int v {1}; // Supported conference protocol version
    int layout {0};
    CallInfoMap callInfo_;

    friend bool operator==(const ConfInfo& p1, const ConfInfo& p2)
    {
        if (p1.h != p2.h or p1.w != p2.w)
            return false;
        return p1.callInfo_.size() == p2.callInfo_.size() and std::equal(p1.callInfo_.begin(), p1.callInfo_.end(), p2.callInfo_.begin());
    }

    friend bool operator!=(const ConfInfo& p1, const ConfInfo& p2)
    {
        return !(p1 == p2);
    }

    void mergeJson(const Json::Value& jsonObj);

    std::vector<std::map<std::string, std::string>> toVectorMapStringString() const;
    std::string toString() const;
};

class CallStreamsManager : public video::VideoGenerator, public video::VideoFramePassiveReader
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
    virtual void muteStream(const std::string& uri, const std::string& deviceId, const std::string& streamId, bool newState, bool local = false);
    virtual bool isMuted(const std::string& uri, const std::string& deviceId, const std::string& streamId, bool onlyLocal = false) const;
    virtual void setActiveStream(const std::string& uri, const std::string& deviceId, const std::string& streamId, bool newState);
    virtual void removeAudioStreams(const std::string& uri, const std::string& deviceId);

    // Per calls
    virtual void setModerator(const std::string& uri, bool newState);
    virtual bool isModerator(std::string_view uri) const;
    virtual void bindCall(const std::shared_ptr<SIPCall>& call, bool isModerator);
    virtual void removeCall(const std::shared_ptr<SIPCall>& call);
    virtual void setHandRaised(const std::string& uri, const std::string& deviceId, bool newState);
    virtual void setRecording(const std::string& uri, const std::string& deviceId, bool newState);

    // Video
    virtual void attachVideo(Observable<std::shared_ptr<MediaFrame>>* frame,
                     const std::string& callId,
                     const std::string& streamId);
    virtual void detachVideo(Observable<std::shared_ptr<MediaFrame>>* frame);

#ifdef ENABLE_VIDEO
    std::shared_ptr<VideoFrameActiveWriter> getVideoLocal() const
    {
        if (!localInputs_.empty())
            return *localInputs_.begin();
        return {};
    }
    /**
     * Stop all inputs
     */
    void stopInput(const std::shared_ptr<VideoFrameActiveWriter>& input);
    void stopInputs();
#endif

    std::set<std::string> getCallIds() const { return callIds_; }

    // For recording
    MediaStream getStream(const std::string& name) const;
    int getWidth() const override { return width_; };
    int getHeight() const override { return height_; };
    AVPixelFormat getPixelFormat() const override { return format_; };

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
    void replaceVideoStream(const std::string& uri, const std::string& deviceId, const std::string& audioStream);

    std::function<void(const CallInfoMap& info)> onInfoUpdated_;
    mutable std::mutex callInfoMtx_;
    CallInfoMap callInfo_;
    std::set<std::string> callIds_;

    std::pair<std::pair<std::string, std::string>, std::string> activeStream_ {};
    Layout currentLayout_ {Layout::GRID};

    std::string uri_;
    std::string deviceId_;

    int width_ = 0;
    int height_ = 0;
    AVPixelFormat format_ = AV_PIX_FMT_YUV422P;
    int64_t lastTimestamp_;

    // Video
    // We need to convert call to frame
    std::map<Observable<std::shared_ptr<MediaFrame>>*, StreamId> videoToStreamId_ {};
    std::map<std::string, Observable<std::shared_ptr<MediaFrame>>*> streamIdToSource_ {};

    std::vector<std::shared_ptr<VideoFrameActiveWriter>> localInputs_ {};
};

} // namespace jami
