/*
 *  Copyright (C) 2004-2016 Savoir-faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
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

#ifndef __VIDEO_RTP_SESSION_H__
#define __VIDEO_RTP_SESSION_H__

#include "media/rtp_session.h"
#include "media/media_device.h"

#include "video_base.h"
#include "threadloop.h"

#include <string>
#include <memory>

namespace ring {
class Conference;
} // namespace ring

namespace ring { namespace video {

class VideoMixer;
class VideoSender;
class VideoReceiveThread;

struct VideoBitrateInfo {
    unsigned videoBitrateCurrent;
    unsigned videoBitrateMin;
    unsigned videoBitrateMax;
    unsigned videoQualityCurrent;
    unsigned videoQualityMin;
    unsigned videoQualityMax;
    float packetLostThreshold;
};

enum QualityState : unsigned {
    QUALITY_UNTESTED = 0,
    QUALITY_KO = 1,
    QUALITY_OK = 2
};

struct QualityInfo {
    unsigned val;
    QualityState state = QUALITY_UNTESTED;
    unsigned statTotal = 0;
    unsigned statFailure = 0;
    unsigned statSuccess = 0;
    std::chrono::time_point<std::chrono::system_clock> timeLastFailure;
    QualityInfo(unsigned val, QualityState state, unsigned statT, unsigned statF, unsigned statS)
        : val(val),
        state(state),
        statTotal(statT),
        statFailure(statF),
        statSuccess(statS),
        timeLastFailure(std::chrono::system_clock::now()) {}
};

class VideoRtpSession : public RtpSession {
public:
    VideoRtpSession(const std::string& callID,
                    const DeviceParams& localVideoParams);
    ~VideoRtpSession();

    void start(std::unique_ptr<IceSocket> rtp_sock = nullptr,
               std::unique_ptr<IceSocket> rtcp_sock = nullptr) override;
    void restartSender() override;
    void stop() override;

    void forceKeyFrame();
    void bindMixer(VideoMixer* mixer);
    void unbindMixer();
    void enterConference(Conference* conference);
    void exitConference();
    void switchInput(const std::string& input) {
        input_ = input;
    }

    bool useCodec(const AccountVideoCodecInfo* codec) const;

private:
    void setupConferenceVideoPipeline(Conference& conference);
    void setupVideoPipeline();
    void startSender();
    void startReceiver();

    std::string input_;
    DeviceParams localVideoParams_;

    std::chrono::time_point<std::chrono::system_clock>  lastRTCPCheck_;
    std::chrono::time_point<std::chrono::system_clock>  lastLongRTCPCheck_;

    std::unique_ptr<VideoSender> sender_;
    std::unique_ptr<VideoReceiveThread> receiveThread_;
    Conference* conference_ {nullptr};
    std::shared_ptr<VideoMixer> videoMixer_;
    std::shared_ptr<VideoFrameActiveWriter> videoLocal_;
    uint16_t initSeqVal_ = 0;

    float checkPeerPacketLoss();
    void adaptQualityAndBitrate();
    void storeVideoBitrateInfo();
    void getVideoBitrateInfo();


    // interval in seconds between RTCP checkings
    const unsigned RTCP_CHECKING_INTERVAL {2};
    // long interval in seconds between RTCP checkings
    const unsigned RTCP_LONG_CHECKING_INTERVAL {30};
    // no packet loss can be calculated as no data in input
    static constexpr float NO_PACKET_LOSS_CALCULATED {-1.0};
    // bitrate and quality info struct
    VideoBitrateInfo videoBitrateInfo_ = {0,0,0,0,0,0,PACKET_LOSS_THRESHOLD};

    static constexpr float PACKET_LOSS_THRESHOLD {1.0};
    static constexpr float PACKET_LOSS_LOW {5.0};
    static constexpr float PACKET_LOSS_MEDIUM {15.0};
    static constexpr float PACKET_LOSS_HIGH {25.0};

    InterruptedThreadLoop rtcpCheckerThread_;
    bool setupRtcpChecker();
    void processRtcpChecker();
    void cleanupRtcpChecker();

    void initBitrateAndQualityTable();
    void resetBitrateAndQualityStats();
    bool increaseQuality();
    bool decreaseQuality(float packetLoss);
    bool increaseBitrate();
    bool decreaseBitrate(float packetLoss);
    void dumpStat();
    void updateQualityTable();
    void updateBitrateTable();

    std::vector<QualityInfo> tableQuality_ {};
    unsigned indexQuality_;
    std::vector<QualityInfo> tableBitrate_ {};
    unsigned indexBitrate_;
};
}} // namespace ring::video

#endif // __VIDEO_RTP_SESSION_H__
