/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include "media/rtp_session.h"
#include "media/media_device.h"

#include "video_base.h"
#include "threadloop.h"

#include <string>
#include <memory>

namespace jami {
class CongestionControl;
class Conference;
class MediaRecorder;
} // namespace jami

namespace jami {
namespace video {

class VideoInput;
class VideoMixer;
class VideoSender;
class VideoReceiveThread;

struct RTCPInfo
{
    float packetLoss;
    unsigned int jitter;
    unsigned int nb_sample;
    float latency;
};

struct VideoBitrateInfo
{
    unsigned videoBitrateCurrent;
    unsigned videoBitrateMin;
    unsigned videoBitrateMax;
    unsigned videoQualityCurrent;
    unsigned videoQualityMin;
    unsigned videoQualityMax;
    unsigned cptBitrateChecking;
    unsigned maxBitrateChecking;
    float packetLostThreshold;
};

class VideoRtpSession : public RtpSession, public std::enable_shared_from_this<VideoRtpSession>
{
public:
    using BaseType = RtpSession;

    VideoRtpSession(const std::string& callId,
                    const std::string& streamId,
                    const DeviceParams& localVideoParams,
                    const std::shared_ptr<MediaRecorder>& rec);
    ~VideoRtpSession();

    void setRequestKeyFrameCallback(std::function<void(void)> cb);

    void updateMedia(const MediaDescription& send, const MediaDescription& receive) override;

    void start(std::unique_ptr<dhtnet::IceSocket> rtp_sock, std::unique_ptr<dhtnet::IceSocket> rtcp_sock) override;
    void restartSender() override;
    void stop() override;
    void setMuted(bool mute, Direction dir = Direction::SEND) override;

    /**
     * Set video orientation
     *
     * Send to the receive thread rotation to apply to the video (counterclockwise)
     *
     * @param rotation Rotation in degrees (counterclockwise)
     */
    void setRotation(int rotation);
    void forceKeyFrame();
    void bindMixer(VideoMixer* mixer);
    void unbindMixer();
    void enterConference(Conference& conference);
    void exitConference();

    void setChangeOrientationCallback(std::function<void(int)> cb);
    void initRecorder() override;
    void deinitRecorder() override;

    const VideoBitrateInfo& getVideoBitrateInfo();

    bool hasConference() { return conference_; }

    std::shared_ptr<VideoInput>& getVideoLocal() { return videoLocal_; }

    std::shared_ptr<VideoMixer>& getVideoMixer() { return videoMixer_; }

    std::shared_ptr<VideoReceiveThread>& getVideoReceive() { return receiveThread_; }

private:
    void setupConferenceVideoPipeline(Conference& conference, Direction dir);
    void setupVideoPipeline();
    void startSender();
    void stopSender(bool forceStopSocket = false);
    void startReceiver();
    void stopReceiver(bool forceStopSocket = false);
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;

    DeviceParams localVideoParams_;

    std::unique_ptr<VideoSender> sender_;
    std::shared_ptr<VideoReceiveThread> receiveThread_;
    Conference* conference_ {nullptr};
    std::shared_ptr<VideoMixer> videoMixer_;
    std::shared_ptr<VideoInput> videoLocal_;
    uint16_t initSeqVal_ = 0;

    std::function<void(void)> requestKeyFrameCallback_;

    bool check_RCTP_Info_RR(RTCPInfo&);
    bool check_RCTP_Info_REMB(uint64_t*);
    void adaptQualityAndBitrate();
    void storeVideoBitrateInfo();
    void setupVideoBitrateInfo();
    void checkReceiver();
    float getPonderateLoss(float lastLoss);
    void delayMonitor(int gradient, int deltaT);
    void dropProcessing(RTCPInfo* rtcpi);
    void delayProcessing(int br);
    void setNewBitrate(unsigned int newBR);

    // no packet loss can be calculated as no data in input
    static constexpr float NO_INFO_CALCULATED {-1.0};
    // bitrate and quality info struct
    VideoBitrateInfo videoBitrateInfo_;
    std::list<std::pair<time_point, float>> histoLoss_;

    // 5 tries in a row
    static constexpr unsigned MAX_ADAPTATIVE_BITRATE_ITERATION {5};
    // packet loss threshold
    static constexpr float PACKET_LOSS_THRESHOLD {1.0};

    InterruptedThreadLoop rtcpCheckerThread_;
    void processRtcpChecker();

    std::function<void(int)> changeOrientationCallback_;

    std::function<void(bool)> recordingStateCallback_;

    // interval in seconds between RTCP checkings
    std::chrono::seconds rtcp_checking_interval {4};

    time_point lastMediaRestart_ {time_point::min()};
    time_point last_REMB_inc_ {time_point::min()};
    time_point last_REMB_dec_ {time_point::min()};
    time_point lastBitrateDecrease {time_point::min()};

    unsigned remb_dec_cnt_ {0};

    std::unique_ptr<CongestionControl> cc;

    std::function<void(void)> cbKeyFrameRequest_;

    std::atomic<int> rotation_ {0};

    void attachRemoteRecorder(const MediaStream& ms);
    void attachLocalRecorder(const MediaStream& ms);
};

} // namespace video
} // namespace jami
