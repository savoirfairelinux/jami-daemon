/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
 *
 *  Author: Alexandre Savard  <alexandre.savard@savoirfairelinux.com>
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <set>
#include <string>
#include <memory>
#include <vector>

#include "audio/audio_input.h"
#include "audio/ringbufferpool.h"

#include <json/json.h>

#include "recordable.h"

namespace jami {

#ifdef ENABLE_VIDEO
namespace video {
class VideoMixer;
}
#endif

struct ParticipantInfo
{
    std::string uri;
    std::string devURI;
    bool active {false};
    int x {0};
    int y {0};
    int w {0};
    int h {0};
    bool videoMuted {false};
    bool audioMuted {false};
    bool isModerator {false};

    void fromJson(const Json::Value& v)
    {
        uri = v["uri"].asString();
        devURI = v["devURI"].asString();
        active = v["active"].asBool();
        x = v["x"].asInt();
        y = v["y"].asInt();
        w = v["w"].asInt();
        h = v["h"].asInt();
        videoMuted = v["videoMuted"].asBool();
        audioMuted = v["audioMuted"].asBool();
        isModerator = v["isModerator"].asBool();
    }

    Json::Value toJson() const
    {
        Json::Value val;
        val["uri"] = uri;
        val["devURI"] = devURI;
        val["active"] = active;
        val["x"] = x;
        val["y"] = y;
        val["w"] = w;
        val["h"] = h;
        val["videoMuted"] = videoMuted;
        val["audioMuted"] = audioMuted;
        val["isModerator"] = isModerator;
        return val;
    }

    std::map<std::string, std::string> toMap() const
    {
        return {{"uri", uri},
                {"devURI", devURI},
                {"active", active ? "true" : "false"},
                {"x", std::to_string(x)},
                {"y", std::to_string(y)},
                {"w", std::to_string(w)},
                {"h", std::to_string(h)},
                {"videoMuted", videoMuted ? "true" : "false"},
                {"audioMuted", audioMuted ? "true" : "false"},
                {"isModerator", isModerator ? "true" : "false"}};
    }
};

struct ConfInfo : public std::vector<ParticipantInfo>
{
    std::vector<std::map<std::string, std::string>> toVectorMapStringString() const;
};

using ParticipantSet = std::set<std::string>;

class Conference : public Recordable, public std::enable_shared_from_this<Conference>
{
public:
    enum class State { ACTIVE_ATTACHED, ACTIVE_DETACHED, HOLD };

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
    const std::string& getConfID() const;

    /**
     * Return the current conference state
     */
    State getState() const;

    /**
     * Set conference state
     */
    void setState(State state);

    /**
     * Return a string description of the conference state
     */
    static constexpr const char* getStateStr(State state)
    {
        switch (state) {
        case State::ACTIVE_ATTACHED:
            return "ACTIVE_ATTACHED";
        case State::ACTIVE_DETACHED:
            return "ACTIVE_DETACHED";
        case State::HOLD:
            return "HOLD";
        default:
            return "";
        }
    }

    const char* getStateStr() const { return getStateStr(confState_); }

    /**
     * Add a new participant to the conference
     */
    void add(const std::string& participant_id);

    /**
     * Remove a participant from the conference
     */
    void remove(const std::string& participant_id);

    /**
     * Attach local audio/video to the conference
     */
    void attach();

    /**
     * Detach local audio/video from the conference
     */
    void detach();

    /**
     * Bind a participant to the conference
     */
    void bindParticipant(const std::string& participant_id);

    /**
     * Bind a participant to the conference
     */
    void bindHost() { bindParticipant(jami::RingBufferPool::DEFAULT_ID); }

    /**
     * unbind a participant from the conference
     */
    void unbindParticipant(const std::string& participant_id);

    /**
     * unbind host from conference
     */
    void unbindHost() { unbindParticipant(jami::RingBufferPool::DEFAULT_ID); }

    /**
     * Get the participant list for this conference
     */
    const ParticipantSet& getParticipantList() const;

    /**
     * Get the display names or peer numbers for this conference
     */
    std::vector<std::string> getDisplayNames() const;

    /**
     * Start/stop recording toggle
     */
    bool toggleRecording() override;

    void switchInput(const std::string& input);

    void setActiveParticipant(const std::string& participant_id);
    void setLayout(int layout);

    void attachVideo(Observable<std::shared_ptr<MediaFrame>>* frame, const std::string& callId);
    void detachVideo(Observable<std::shared_ptr<MediaFrame>>* frame);

    void onConfOrder(const std::string& callId, const std::string& order);

#ifdef ENABLE_VIDEO
    std::shared_ptr<video::VideoMixer> getVideoMixer();
    std::string getVideoInput() const { return mediaInput_; }
#endif

    std::vector<std::map<std::string, std::string>> getConferenceInfos() const
    {
        std::lock_guard<std::mutex> lk(confInfoMutex_);
        return confInfo_.toVectorMapStringString();
    }

    void setModerator(const std::string& uri, const bool& state);
    void muteParticipant(const std::string& uri, const bool& state);

private:
    std::weak_ptr<Conference> weak()
    {
        return std::static_pointer_cast<Conference>(shared_from_this());
    }

    bool isModerator(const std::string_view uri) const;
    void updateModerators();

    std::string id_;
    State confState_ {State::ACTIVE_ATTACHED};
    ParticipantSet participants_;

    mutable std::mutex confInfoMutex_ {};
    mutable ConfInfo confInfo_ {};
    void sendConferenceInfos();
    // We need to convert call to frame
    std::mutex videoToCallMtx_;
    std::map<Observable<std::shared_ptr<MediaFrame>>*, std::string> videoToCall_ {};
    std::shared_ptr<RingBuffer> ghostRingBuffer_;

#ifdef ENABLE_VIDEO
    std::string mediaInput_ {};
    std::shared_ptr<video::VideoMixer> videoMixer_;
#endif

    std::shared_ptr<jami::AudioInput> audioMixer_;
    std::set<std::string> moderators_ {};
    std::set<std::string> participantsMuted_ {};

    void initRecorder(std::shared_ptr<MediaRecorder>& rec);
    void deinitRecorder(std::shared_ptr<MediaRecorder>& rec);

    bool isMuted(const std::string_view uri) const;
    void updateMuted();

    ConfInfo getConfInfoHostUri(const std::string_view uri);
    bool isHost(const std::string_view uri) const;
};

} // namespace jami
