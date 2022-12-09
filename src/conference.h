/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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

#include <chrono>
#include <memory>
#include <string_view>

#include "audio/audio_input.h"
#include "callstreamsmanager.h"
#include "conference_protocol.h"
#include "media/media_attribute.h"
#include "media/recordable.h"

#ifdef ENABLE_PLUGIN
#include "plugin/streamdata.h"
#endif

#ifdef ENABLE_VIDEO
#include "media/video/sinkclient.h"
#endif

#include <json/json.h>

namespace jami {

class Call;
class Account;

#ifdef ENABLE_VIDEO
namespace video {
class VideoMixer;
}
#endif

using clock = std::chrono::steady_clock;

/**
 * This objects has 2 objectives:
 * 1. Manage medias (request medias changes) and host state
 * 2. Transmit informations between upper layers and components
 *      - network events are from the ConfProtocolParser
 *      - layout events are from the CallStreamsManager
 */
class Conference : public Recordable, public std::enable_shared_from_this<Conference>
{
public:
    enum class State { ACTIVE_ATTACHED, ACTIVE_DETACHED, HOLD };

    /**
     * Constructor for this class, increment static counter
     * @param account       Linked account
     * @param confId        If specified, this conf will use a specific id
     * @param attachHost    If we want to start attached
     * @param hostAttr      Default medias we want to use (we can start in audio-only)
     */
    explicit Conference(const std::shared_ptr<Account>& account,
                        const std::string& confId = "",
                        bool attachHost = true,
                        const std::vector<MediaAttribute>& hostAttr = {});
    void initLayout();
    ~Conference();

    /**
     * @return Conference duration in milliseconds
     */
    std::chrono::milliseconds getDuration() const
    {
        return duration_start_ == clock::time_point::min()
                   ? std::chrono::milliseconds::zero()
                   : std::chrono::duration_cast<std::chrono::milliseconds>(clock::now()
                                                                           - duration_start_);
    }

    /**
     * Return the conference id
     */
    const std::string& getConfId() const { return id_; }
    /**
     * Get linked accountId
     */
    std::string getAccountId() const { return accountId_; };

    /**
     * Set conference state
     */
    void setState(State state);

    /**
     * Return the current conference state
     */
    State getState() const { return confState_; };
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
     * Set a callback that will be called when the conference will be destroyed
     */
    void onShutdown(std::function<void(int)> cb) { shutdownCb_ = std::move(cb); }

    // Layout informations (passed to CallStreamsManager)
    std::shared_ptr<CallStreamsManager> callStreamsMgr() const { return callStreamsMgr_; }
    void setVoiceActivity(const std::string& uri, const std::string& deviceId, const std::string& streamId, const bool& newState);
    void setLocalMuteState(const std::string& uri, const std::string& deviceId, const std::string& streamId, const bool& newState);
    void setHandRaised(const std::string& uri, const std::string& deviceId, const bool& state);
    void setActiveParticipant(const std::string& callId);
    void setRecording(const std::string& uri, const std::string& deviceId, bool state);
    void setActiveStream(const std::string& uri, const std::string& deviceId, const std::string& streamId, bool state);
    void setLayout(int layout);
    void setModerator(const std::string& uri, const bool& state);
    void hangupParticipant(const std::string& accountUri, const std::string& deviceId = "");
    /**
     * The client shows one tile per stream (video/audio related to a media)
     * @note for now, in conferences we can only mute the audio of a call
     * @todo add a track (audio OR video) parameter to know what we want to mute
     * @param accountUri        Account of the stream
     * @param deviceId          Device of the stream
     * @param streamId          Stream to mute
     * @param state             True to mute, false to unmute
     */
    void muteStream(const std::string& accountUri,
                    const std::string& deviceId,
                    const std::string& streamId,
                    const bool& state);

    /**
     * Remove all audio streams
     * @param uri       Uri of the peer
     * @param deviceId  Remote deviceId
     */
    void removeAudioStreams(const std::string& uri, const std::string& deviceId);

    // Media management
    /**
     * Get the mute state of the local host
     */
    bool isMediaSourceMuted(MediaType type) const;

    /**
     * Process a media change request.
     * Used to change the media attributes of the host.
     *
     * @param remoteMediaList new media list from the remote
     * @return true on success
     */
    bool requestMediaChange(const std::vector<libjami::MediaMap>& mediaList);

    /**
     * Process incoming media change request.
     *
     * @param callId the call ID
     * @param remoteMediaList new media list from the remote
     */
    void handleMediaChangeRequest(const std::shared_ptr<Call>& call,
                                  const std::vector<libjami::MediaMap>& remoteMediaList);

    /**
     * Add a new call to the conference
     */
    void bindCall(const std::shared_ptr<SIPCall>& call);
    /**
     * Bind a call to the conference
     */
    void bindCallId(const std::string& callId);

    /**
     * Remove a call from the conference
     */
    void removeCall(const std::shared_ptr<SIPCall>& call);

    /**
     * Attach local audio/video to the conference
     */
    void attachLocal();

    /**
     * Detach local audio/video from the conference
     */
    void detachLocal();

    /**
     * Bind host to the conference
     */
    void bindHost();

    /**
     * unbind a call from the conference
     */
    void unbindCallId(const std::string& callId);

    /**
     * unbind host from conference
     */
    void unbindHost();

    /**
     * Get the call list for this conference
     */
    std::set<std::string> getCallIds() const;

    /**
     * Start/stop recording toggle
     */
    bool toggleRecording() override;
    bool startRecording(const std::string& path) override;
    void stopRecording() override;

    /**
     * Mute a call
     */
    void muteParticipant(const std::string& uri, const bool& state);
    void muteLocalHost(bool is_muted, const std::string& mediaType);

    /**
     * Announce to the client that medias are successfully negotiated
     */
    void reportMediaNegotiationStatus();

    /**
     * Retrieve current medias list
     * @return current medias
     */
    std::vector<libjami::MediaMap> currentMediaList() const;

    // Parser informations
    /**
     * Triggered when a network information is received from another peer
     * @param callId        Sender
     * @param order         The json
     */
    void onConfOrder(const std::string& callId, const std::string& order);
    /**
     * If a remote host sends some conference informations
     * @param newInfo
     * @param callId
     */
    void mergeConfInfo(ConfInfo& newInfo, const std::string& callid);

#ifdef ENABLE_VIDEO
    void createSinks(const ConfInfo& infos);
    std::string getVideoInput() const;
#endif

    std::vector<std::map<std::string, std::string>> getConferenceInfos() const
    {
        std::lock_guard<std::mutex> lk(confInfoMutex_);
        return confInfo_.toVectorMapStringString();
    }

private:
    std::string id_;
    std::weak_ptr<Account> account_;
    std::string accountId_;
    std::string accountUri_;
    std::string accountDevice_;
    State confState_ {State::ACTIVE_ATTACHED};

    std::weak_ptr<Conference> weak()
    {
        return std::static_pointer_cast<Conference>(shared_from_this());
    }

    static std::shared_ptr<Call> getCall(const std::string& callId);
    void foreachCall(const std::function<void(const std::shared_ptr<Call>& call)>& cb);
    std::shared_ptr<Call> getCallWith(const std::string& accountUri, const std::string& deviceId = "");

    void muteHost(bool state);
    void muteCall(const std::string& callId, bool state);

    std::string mediaPlayerId_ {};
    /**
     * If the local host is participating in the conference (attached
     * mode ), this variable will hold the media source states
     * of the local host.
     */
    mutable std::mutex hostSourcesMtx_;
    std::vector<MediaAttribute> hostSources_;
    /**
     * Set default media source for the local host
     */
    void setLocalHostDefaultMediaSource();
    /**
     * Take over media control from the call.
     * When a call joins a conference, the media control (mainly mute/un-mute
     * state of the local media source) will be handled by the conference and
     * the mixer.
     */
    void takeOverMediaSourceControl(const std::string& callId);

    std::shared_ptr<RingBuffer> ghostRingBuffer_;
    std::shared_ptr<jami::AudioInput> audioMixer_;
#ifdef ENABLE_VIDEO
    std::mutex sinksMtx_ {};
    std::map<std::string, std::shared_ptr<video::SinkClient>> confSinksMap_ {};
    void resizeRemoteParticipants(ConfInfo& confInfo, const std::string& callId);
#endif

    void initRecorder(std::shared_ptr<MediaRecorder>& rec);
    void deinitRecorder(std::shared_ptr<MediaRecorder>& rec);

    mutable std::mutex confInfoMutex_ {};
    ConfInfo confInfo_ {};
    void sendConferenceInfos();
    ConfInfo getConfInfoHostUri(std::string_view localHostURI = "", const std::string& peerUri = "", const std::string& peerDevice = "");
    bool isHost(std::string_view uri) const;
    // Used for merging confInfo
    std::map<std::pair<std::string, std::string>, ConfInfo> remoteHosts_;
    std::pair<std::string, std::string> findHostforRemoteParticipant(std::string_view uri,
                                                  std::string_view deviceId = "");

    ConfProtocolParser parser_;
    std::shared_ptr<CallStreamsManager> callStreamsMgr_;
    std::atomic_bool destroying_ {false};

    std::function<void(int)> shutdownCb_;
    clock::time_point duration_start_;
    bool attachHost_;

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
