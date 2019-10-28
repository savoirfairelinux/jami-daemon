/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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

#include "socket_pair.h"

namespace jami {

enum BandwidthUsage {
  bwNormal = 0,
  bwUnderusing = 1,
  bwOverusing = 2
};

// Receiver Estimated Max Bitrate (REMB) (draft-alvestrand-rmcat-remb).
class CongestionControl {
public:
    CongestionControl();
    ~CongestionControl();

    uint64_t parseREMB(rtcpREMBHeader* packet);
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

    float last_estimate_m_ {0.0f};
    float last_var_p_ {0.1f};
    float last_var_n_ {0.0f};

    float last_thresh_y_ {2.0f};

    unsigned overuse_counter_;
    time_point t0_overuse {time_point::min()};

    BandwidthUsage last_state_;

};

} // namespace jami
#endif
