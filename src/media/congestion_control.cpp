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

#include "logger.h"
#include "media/congestion_control.h"

#include <cstdint>
#include <utility>
#include <cmath>

namespace jami {
static constexpr uint8_t packetVersion = 2;
static constexpr uint8_t packetFMT = 15;
static constexpr uint8_t packetType = 206;
static constexpr uint32_t uniqueIdentifier = 0x52454D42;  // 'R' 'E' 'M' 'B'.


static constexpr float Q = 0.5f;
static constexpr float beta = 0.95f;

static constexpr float ku = 0.004f;
static constexpr float kd = 0.002f;

constexpr auto OVERUSE_THRESH = std::chrono::milliseconds(100);


// Receiver Estimated Max Bitrate (REMB) (draft-alvestrand-rmcat-remb).
//
//     0                   1                   2                   3
//     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |V=2|P| FMT=15  |   PT=206      |             length            |
//    +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//  0 |                  SSRC of packet sender                        |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  4 |                       Unused = 0                              |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  8 |  Unique identifier 'R' 'E' 'M' 'B'                            |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 12 |  Num SSRC     | BR Exp    |  BR Mantissa                      |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 16 |   SSRC feedback                                               |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    :  ...                                                          :

CongestionControl::CongestionControl()
{}

CongestionControl::~CongestionControl()
{}

static void
insert2Byte(std::vector<uint8_t> *v, uint16_t val)
{
    v->insert(v->end(), val >> 8);
    v->insert(v->end(), val & 0xff);
}

static void
insert4Byte(std::vector<uint8_t> *v, uint32_t val)
{
    v->insert(v->end(), val >> 24);
    v->insert(v->end(), (val >> 16) & 0xff);
    v->insert(v->end(), (val >> 8) & 0xff);
    v->insert(v->end(), val & 0xff);
}


uint64_t
CongestionControl::parseREMB(rtcpREMBHeader* packet)
{
    if(packet->fmt != 15 || packet->pt != 206) {
        JAMI_ERR("Unable to parse REMB packet.");
        return 0;
    }
    uint64_t bitrate_bps = (packet->br_mantis << packet->br_exp);
    bool shift_overflow = (bitrate_bps >> packet->br_exp) != packet->br_mantis;
    if (shift_overflow) {
        JAMI_ERR("Invalid remb bitrate value : %u*2^%u", packet->br_mantis, packet->br_exp);
        return false;
    }
    return bitrate_bps;
}

std::vector<uint8_t>
CongestionControl::createREMB(uint64_t bitrate_bps)
{
    std::vector<uint8_t> remb;

    remb.insert(remb.end(), packetVersion << 6 | packetFMT);
    remb.insert(remb.end(), packetType);
    insert2Byte(&remb, 5); // (sizeof(rtcpREMBHeader)/4)-1 -> not safe
    insert4Byte(&remb, 0x12345678);     //ssrc
    insert4Byte(&remb, 0x0);     //ssrc source
    insert4Byte(&remb, uniqueIdentifier);     //uid
    remb.insert(remb.end(), 1);                     //n_ssrc

    const uint32_t maxMantissa = 0x3ffff;  // 18 bits.
    uint64_t mantissa = bitrate_bps;
    uint8_t exponenta = 0;
    while (mantissa > maxMantissa) {
        mantissa >>= 1;
        ++exponenta;
    }

    remb.insert(remb.end(), (exponenta << 2) | (mantissa >> 16));
    insert2Byte(&remb, mantissa & 0xffff);
    insert4Byte(&remb, 0x2345678b);

    return std::move(remb);
}


float
CongestionControl::kalmanFilter(uint64_t gradiant_delay)
{
    float var_n = get_var_n(gradiant_delay);
    float k = get_gain_k(Q, var_n);
    float m = get_estimate_m(k, gradiant_delay);
    last_var_p_ = get_sys_var_p(k, Q);
    last_estimate_m_ = m;
    last_var_n_ = var_n;

    return m;
}

float
CongestionControl::get_estimate_m(float k, int d_m)
{
    // JAMI_WARN("[get_estimate_m]k:%f, last_estimate_m_:%f, d_m:%f", k, last_estimate_m_, d_m);
    // JAMI_WARN("m: %f", ((1-k) * last_estimate_m_) + (k * d_m));
    return ((1-k) * last_estimate_m_) + (k * d_m);
}

float
CongestionControl::get_gain_k(float q, float dev_n)
{
    // JAMI_WARN("k: %f", (last_var_p_ + q) / (last_var_p_ + q + dev_n));
    return (last_var_p_ + q) / (last_var_p_ + q + dev_n);
}

float
CongestionControl::get_sys_var_p(float k, float q)
{
    // JAMI_WARN("var_p: %f", ((1-k) * (last_var_p_ + q)));
    return ((1-k) * (last_var_p_ + q));
}

float
CongestionControl::get_var_n(int d_m)
{
    float z = get_residual_z(d_m);
    // JAMI_WARN("var_n: %f", (beta * last_var_n_) + ((1.0f - beta) * z * z));
    return (beta * last_var_n_) + ((1.0f - beta) * z * z);
}

float
CongestionControl::get_residual_z(float d_m)
{
    // JAMI_WARN("z: %f", d_m - last_estimate_m_);
    return (d_m - last_estimate_m_);
}

float
CongestionControl::update_thresh(float m, int deltaT)
{
    float ky = 0.0f;
    if (std::fabs(m) < last_thresh_y_)
        ky = kd;
    else
        ky = ku;
    float res = last_thresh_y_ + ((deltaT * ky) * (std::fabs(m) - last_thresh_y_));
    last_thresh_y_ = res;
    return res;
}

float
CongestionControl::get_thresh()
{
    return last_thresh_y_;
}

BandwidthUsage
CongestionControl::get_bw_state(float estimation, float thresh)
{
    if(estimation > thresh) {
        // JAMI_WARN("Enter overuse state");
        if(not overuse_counter_) {
            t0_overuse = clock::now();
            overuse_counter_++;
            return bwNormal;
        }
        overuse_counter_++;
        time_point now = clock::now();
        auto overuse_timer = now - t0_overuse;
        if((overuse_timer >= OVERUSE_THRESH) and (overuse_counter_ > 1) ) {
            overuse_counter_ = 0;
            last_state_ = bwOverusing;
        }
    }
    else if(estimation < -thresh) {
        // JAMI_WARN("Enter underuse state");
        overuse_counter_ = 0;
        last_state_ = bwUnderusing;
    }
    else {
        overuse_counter_ = 0;
        last_state_ = bwNormal;
    }
    return last_state_;
}

} // namespace jami