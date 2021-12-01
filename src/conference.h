/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
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

#include "config.h"

#include <set>
#include <string>
#include <memory>
#include <vector>
#include <string_view>
#include <map>
#include <functional>

#include "audio/audio_input.h"
#include "media_attribute.h"

#include <json/json.h>

#include "recordable.h"

#ifdef ENABLE_PLUGIN
#include "plugin/streamdata.h"
#endif

#ifdef ENABLE_VIDEO
#include <video/sinkclient.h>
#endif

namespace jami {

class Call;
class Account;

#ifdef ENABLE_VIDEO
namespace video {
class VideoMixer;
}
#endif

struct ParticipantInfo
{
    std::string uri;
    std::string device;
    std::string sinkId;
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
                {"handRaised", handRaised ? "true" : "false"}};
    }

    friend bool operator==(const ParticipantInfo& p1, const ParticipantInfo& p2)
    {
        return p1.uri == p2.uri and p1.device == p2.device and p1.sinkId == p2.sinkId
               and p1.active == p2.active and p1.x == p2.x and p1.y == p2.y and p1.w == p2.w
               and p1.h == p2.h and p1.videoMuted == p2.videoMuted
               and p1.audioLocalMuted == p2.audioLocalMuted
               and p1.audioModeratorMuted == p2.audioModeratorMuted
               and p1.isModerator == p2.isModerator and p1.handRaised == p2.handRaised;
    }

    friend bool operator!=(const ParticipantInfo& p1, const ParticipantInfo& p2)
    {
        return !(p1 == p2);
    }
};

struct ConfInfo : public std::vector<ParticipantInfo>
{
    int h {0};
    int w {0};

    friend bool operator==(const ConfInfo& c1, const ConfInfo& c2)
    {
        if (c1.h != c2.h or c1.w != c2.w)
            return false;
        if (c1.size() != c2.size())
            return false;

        for (auto& p1 : c1) {
            auto it = std::find_if(c2.begin(), c2.end(), [&p1](const ParticipantInfo& p2) {
                return p1 == p2;
            });
            if (it != c2.end())
                continue;
            else
                return false;
        }
        return true;
    }

    friend bool operator!=(const ConfInfo& c1, const ConfInfo& c2) { return !(c1 == c2); }

    std::vector<std::map<std::string, std::string>> toVectorMapStringString() const;
    std::string toString() const;
};

using ParticipantSet = std::set<std::string>;

class Conference : public Recordable, public std::enable_shared_from_this<Conference>
{
public:
    enum class State { ACTIVE_ATTACHED, ACTIVE_DETACHED, HOLD };

    /**
     * Constructor for this class, increment static counter
     */
    explicit Conference(const std::shared_ptr<Account>&);

    /**
     * Destructor for this class, decrement static counter
     */
    ~Conference();

    /**
     * Return the conference id
     */
    const std::string& getConfId() const { return id_; }

    std::shared_ptr<Account> getAccount() const { return account_.lock(); }

    std::string getAccountId() const;

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
     * Set default media source for the local host
     */
    void setLocalHostDefaultMediaSource();

    /**
     * Set the mute state of the local host
     */
    void setLocalHostMuteState(MediaType type, bool muted);

    /**
     * Get the mute state of the local host
     */
    bool isMediaSourceMuted(MediaType type) const;

    /**
     * Take over media control from the call.
     * When a call joins a conference, the media control (mainly mute/un-mute
     * state of the local media source) will be handled by the conference and
     * the mixer.
     */
    void takeOverMediaSourceControl(const std::string& callId);

    /**
     * Process a media change request.
     * Used to change the media attributes of the host.
     *
     * @param remoteMediaList new media list from the remote
     * @return true on success
     */
    bool requestMediaChange(const std::vector<DRing::MediaMap>& mediaList);

    /**
     * Process incoming media change request.
     *
     * @param callId the call ID
     * @param remoteMediaList new media list from the remote
     */
    void handleMediaChangeRequest(const std::shared_ptr<Call>& call,
                                  const std::vector<DRing::MediaMap>& remoteMediaList);

    /**
     * Add a new participant to the conference
     */
    void addParticipant(const std::string& participant_id);

    /**
     * Remove a participant from the conference
     */
    void removeParticipant(const std::string& participant_id);

    /**
     * Attach local audio/video to the conference
     */
    void attachLocalParticipant();

    /**
     * Detach local audio/video from the conference
     */
    void detachLocalParticipant();

    /**
     * Bind a participant to the conference
     */
    void bindParticipant(const std::string& participant_id);

    /**
     * Bind host to the conference
     */
    void bindHost();

    /**
     * unbind a participant from the conference
     */
    void unbindParticipant(const std::string& participant_id);

    /**
     * unbind host from conference
     */
    void unbindHost();

    /**
     * Get the participant list for this conference
     */
    ParticipantSet getParticipantList() const;

    /**
     * Start/stop recording toggle
     */
    bool toggleRecording() override;

    void switchInput(const std::string& input);
    void switchSecondaryInput(const std::string& input);

    void setActiveParticipant(const std::string& participant_id);
    void setLayout(int layout);

    void attachVideo(Observable<std::shared_ptr<MediaFrame>>* frame, const std::string& callId);
    void detachVideo(Observable<std::shared_ptr<MediaFrame>>* frame);

    void onConfOrder(const std::string& callId, const std::string& order);

    bool isVideoEnabled() const;

#ifdef ENABLE_VIDEO
    std::shared_ptr<video::VideoMixer> getVideoMixer();
    std::string getVideoInput() const { return hostVideoSource_.sourceUri_; }
#endif

    std::vector<std::map<std::string, std::string>> getConferenceInfos() const
    {
        std::lock_guard<std::mutex> lk(confInfoMutex_);
        return confInfo_.toVectorMapStringString();
    }

    void updateConferenceInfo(ConfInfo confInfo);
    void createSinks(const ConfInfo& infos);
    void setModerator(const std::string& uri, const bool& state);
    void setHandRaised(const std::string& uri, const bool& state);
    void muteParticipant(const std::string& uri, const bool& state);
    void hangupParticipant(const std::string& participant_id);
    void updateMuted();
    void muteLocalHost(bool is_muted, const std::string& mediaType);
    bool isRemoteParticipant(const std::string& uri);
    void mergeConfInfo(ConfInfo& newInfo, const std::string& peerURI);

private:
    std::weak_ptr<Conference> weak()
    {
        return std::static_pointer_cast<Conference>(shared_from_this());
    }

    static std::shared_ptr<Call> getCall(const std::string& callId);
    bool isModerator(std::string_view uri) const;
    bool isHandRaised(std::string_view uri) const;
    void updateModerators();
    void updateHandsRaised();

    void foreachCall(const std::function<void(const std::shared_ptr<Call>& call)>& cb);

    std::string id_;
    std::weak_ptr<Account> account_;
    State confState_ {State::ACTIVE_ATTACHED};
    mutable std::mutex participantsMtx_ {};
    ParticipantSet participants_;

    mutable std::mutex confInfoMutex_ {};
    ConfInfo confInfo_ {};

    void sendConferenceInfos();
    // We need to convert call to frame
    std::mutex videoToCallMtx_;
    std::map<Observable<std::shared_ptr<MediaFrame>>*, std::string> videoToCall_ {};
    std::shared_ptr<RingBuffer> ghostRingBuffer_;

#ifdef ENABLE_VIDEO
    bool videoEnabled_;
    std::string mediaSecondaryInput_ {};
    std::shared_ptr<video::VideoMixer> videoMixer_;
    std::map<std::string, std::shared_ptr<video::SinkClient>> confSinksMap_ {};
#endif

    std::shared_ptr<jami::AudioInput> audioMixer_;
    std::set<std::string, std::less<>> moderators_ {};
    std::set<std::string, std::less<>> participantsMuted_ {};
    std::set<std::string, std::less<>> handsRaised_;

    void initRecorder(std::shared_ptr<MediaRecorder>& rec);
    void deinitRecorder(std::shared_ptr<MediaRecorder>& rec);

    bool isMuted(std::string_view uri) const;

    ConfInfo getConfInfoHostUri(std::string_view localHostURI, std::string_view destURI);
    bool isHost(std::string_view uri) const;

    /**
     * If the local host is participating in the conference (attached
     * mode ), these two  variables will hold the media source states
     * of the local host.
     *
     * NOTE:
     * Currently, the conference and the client support only one stream
     * per media type, even if the call supports an arbitrary number of
     * streams per media type. Thus, these two variables will hold the
     * current media source attributes
     */
    MediaAttribute hostAudioSource_ {};
#ifdef ENABLE_VIDEO
    MediaAttribute hostVideoSource_ {};
#endif

    bool localModAdded_ {false};

    std::map<std::string, ConfInfo> remoteHosts_;
    void resizeRemoteParticipants(ConfInfo& confInfo, std::string_view peerURI);
    std::string_view findHostforRemoteParticipant(std::string_view uri);
    std::shared_ptr<Call> getCallFromPeerID(std::string_view peerID);

    std::mutex sinksMtx_ {};

#ifdef ENABLE_PLUGIN
    /**
     * Call Streams and some typedefs
     */
    using AVMediaStream = Observable<std::shared_ptr<MediaFrame>>;
    using MediaStreamSubject = PublishMapSubject<std::shared_ptr<MediaFrame>, AVFrame*>;

#ifdef ENABLE_VIDEO
    /**
     *   Map: maps the VideoFrame to an AVFrame
     **/
    std::function<AVFrame*(const std::shared_ptr<jami::MediaFrame>&)> pluginVideoMap_ =
        [](const std::shared_ptr<jami::MediaFrame>& m) -> AVFrame* {
        return std::static_pointer_cast<VideoFrame>(m)->pointer();
    };
#endif // ENABLE_VIDEO

    /**
     * @brief createConfAVStream
     * Creates a conf AV stream like video input, video receive, audio input or audio receive
     * @param StreamData
     * @param streamSource
     * @param mediaStreamSubject
     */
    void createConfAVStream(const StreamData& StreamData,
                            AVMediaStream& streamSource,
                            const std::shared_ptr<MediaStreamSubject>& mediaStreamSubject,
                            bool force = false);
    /**
     * @brief createConfAVStreams
     * Creates all Conf AV Streams (2 if audio, 4 if audio video)
     */
    void createConfAVStreams();

    std::mutex avStreamsMtx_ {};
    std::map<std::string, std::shared_ptr<MediaStreamSubject>> confAVStreams;
#endif // ENABLE_PLUGIN
};

} // namespace jami
