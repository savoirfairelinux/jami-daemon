/*
 *  Copyright (C) 2004-2015 Savoir-faire Linux Inc.
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

#include <string>
#include <memory>

namespace ring {
class Conference;
class Call;
} // namespace ring

namespace ring { namespace video {

class VideoMixer;
class VideoSender;
class VideoReceiveThread;

struct VideoBitrateInfo {
    unsigned videoBitrateCurrent;
    unsigned videoBitrateMin;
    unsigned videoBitrateMax;
    unsigned cptBitrateChecking;
    unsigned maxBitrateChecking;
    float packetLostThreshold;
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
    void startSender();
    void startReceiver();

    std::string input_;
    DeviceParams localVideoParams_;
    //std::shared_ptr<Call> call_;
    std::chrono::time_point<std::chrono::system_clock>  lastRTCPCheck_;
    std::chrono::time_point<std::chrono::system_clock>  lastLongRTCPCheck_;

    std::unique_ptr<VideoSender> sender_;
    std::unique_ptr<VideoReceiveThread> receiveThread_;
    Conference* conference_ {nullptr};
    std::shared_ptr<VideoMixer> videoMixer_;
    std::shared_ptr<VideoFrameActiveWriter> videoLocal_;
    uint16_t initSeqVal_ = 0;

    float checkPeerPacketLoss();
    void adaptBitrate();
    void storeVideoBitrateInfo();
    void getVideoBitrateInfo();


    //interval in seconds between RTCP checkings
    const unsigned RTCP_CHECKING_INTERVAL {4};
    //long interval in seconds between RTCP checkings
    const unsigned RTCP_LONG_CHECKING_INTERVAL {30};
    //not packet loss can be calculated as no data in input
    static constexpr float NO_PACKET_LOSS_CALCULATED {-1.0};
    //bitrate info struct
    VideoBitrateInfo videoBitrateInfo_ = {0,0,0,0, MAX_ADAPTATIVE_BITRATE_ITERATION, PACKET_LOSS_THRESHOLD};
    //5 tries in a row
    static constexpr unsigned  MAX_ADAPTATIVE_BITRATE_ITERATION {5};
    //packet loss threshold
    static constexpr float PACKET_LOSS_THRESHOLD {1.0};

    ThreadLoop rtcpCheckerThread_;
    bool setupRtcpChecker();
    void processRtcpChecker();
    void cleanupRtcpChecker();

};

}} // namespace ring::video

#endif // __VIDEO_RTP_SESSION_H__
