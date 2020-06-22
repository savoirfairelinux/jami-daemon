/*
 *  Copyright (C) 2004-2020 Savoir-faire Linux Inc.
 *
 *  Author: Pierre Lespagnol <pierre.lespagnol@savoirfairelinux.com>
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

#ifndef MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_REMB_H_
#define MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_REMB_H_

#include <vector>
#include <cstdint>
#include <memory>

#include "socket_pair.h"
#include "audio/audio_rtp_session.h"
#include "video/video_rtp_session.h"

namespace jami {

enum BandwidthUsage {
    bwNormal = 0,
    bwUnderusing = 1,
    bwOverusing = 2
};

struct RTCPInfo {
    float packetLoss;
    unsigned int jitter;
    unsigned int nb_sample;
    float latency;
};

// Receiver Estimated Max Bitrate (REMB) (draft-alvestrand-rmcat-remb).
class CongestionControl {
using VideoRtpSession = video::VideoRtpSession;

public:
    CongestionControl();
    CongestionControl(std::shared_ptr<AudioRtpSession> artp, std::shared_ptr<VideoRtpSession> vrtp);
    ~CongestionControl();

    uint64_t parseREMB(const rtcpREMBHeader &packet);
    std::vector<uint8_t> createREMB(uint64_t bitrate_bps);
    float kalmanFilter(uint64_t gradiant_delay);
    float update_thresh(float m, int deltaT);
    float get_thresh();
    BandwidthUsage get_bw_state(float estimation, float thresh);

private:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;

    float get_estimate_m(float k, int d_m);
    float get_gain_k(float q, float dev_n);
    float get_sys_var_p(float k, float q);
    float get_var_n(int d_m);
    float get_residual_z(float d_m);
    void delayMonitor(int gradient, int deltaT);
    bool check_RCTP_Info_REMB(uint64_t*);
    void adaptQualityAndBitrate();
    void delayProcessing(int br);
    bool check_RCTP_Info_RR(RTCPInfo&);
    void dropProcessing(RTCPInfo* rtcpi);
    float getPonderateLoss(float lastLoss);

    float last_estimate_m_ {0.0f};
    float last_var_p_ {0.1f};
    float last_var_n_ {0.0f};

    float last_thresh_y_ {2.0f};

    unsigned overuse_counter_;
    time_point t0_overuse {time_point::min()};

    BandwidthUsage last_state_;

    std::shared_ptr<AudioRtpSession> artp_;
    std::shared_ptr<VideoRtpSession> vrtp_;

    time_point last_REMB_inc_ {time_point::min()};
    time_point last_REMB_dec_ {time_point::min()};
    time_point lastMediaRestart_ {time_point::min()};

    unsigned remb_dec_cnt_ {0};
    std::list< std::pair<time_point, float> > histoLoss_;

    InterruptedThreadLoop rtcpCheckerThread_;
    void processRtcpChecker();


};

} // namespace jami
#endif
