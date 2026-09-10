/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
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

#include "logger.h"
#include "media/congestion_control.h"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <limits>

namespace jami {
static constexpr uint8_t packetVersion = 2;
static constexpr uint8_t packetFMT = 15;
static constexpr uint8_t packetType = 206;
static constexpr uint32_t uniqueIdentifier = 0x52454D42; // 'R' 'E' 'M' 'B'.

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

CongestionControl::CongestionControl() {}

CongestionControl::~CongestionControl() {}

static void
insert2Byte(std::vector<uint8_t>& v, uint16_t val)
{
    v.insert(v.end(), val >> 8);
    v.insert(v.end(), val & 0xff);
}

static void
insert4Byte(std::vector<uint8_t>& v, uint32_t val)
{
    v.insert(v.end(), val >> 24);
    v.insert(v.end(), (val >> 16) & 0xff);
    v.insert(v.end(), (val >> 8) & 0xff);
    v.insert(v.end(), val & 0xff);
}

static uint32_t
read4Byte(const uint8_t* v)
{
    return (uint32_t(v[0]) << 24) | (uint32_t(v[1]) << 16) | (uint32_t(v[2]) << 8) | uint32_t(v[3]);
}

uint64_t
CongestionControl::parseREMB(const rtcpREMBHeader& packet)
{
    const auto* bytes = reinterpret_cast<const uint8_t*>(&packet);
    const auto version = bytes[0] >> 6;
    const auto fmt = bytes[0] & 0x1f;
    const auto brExp = bytes[17] >> 2;
    const auto brMantissa = (uint32_t(bytes[17] & 0x03) << 16) | (uint32_t(bytes[18]) << 8) | uint32_t(bytes[19]);

    if (version != packetVersion || fmt != packetFMT || bytes[1] != packetType
        || read4Byte(bytes + 12) != uniqueIdentifier) {
        JAMI_ERROR("Unable to parse REMB packet.");
        return 0;
    }

    uint64_t bitrate_bps = (uint64_t(brMantissa) << brExp);
    bool shift_overflow = (bitrate_bps >> brExp) != brMantissa;
    if (shift_overflow) {
        JAMI_ERROR("Invalid remb bitrate value : {}*2^{}", brMantissa, brExp);
        return 0;
    }
    return bitrate_bps;
}

std::vector<uint8_t>
CongestionControl::createREMB(uint64_t bitrate_bps, uint32_t senderSsrc, const std::vector<uint32_t>& feedbackSsrcs)
{
    std::vector<uint8_t> remb;
    if (feedbackSsrcs.empty() || feedbackSsrcs.size() > std::numeric_limits<uint8_t>::max()) {
        JAMI_ERROR("Unable to create REMB packet with {} feedback SSRCs", feedbackSsrcs.size());
        return remb;
    }

    const auto packetSize = 20u + 4u * static_cast<unsigned>(feedbackSsrcs.size());
    remb.reserve(packetSize);

    remb.insert(remb.end(), packetVersion << 6 | packetFMT);
    remb.insert(remb.end(), packetType);
    insert2Byte(remb, static_cast<uint16_t>(packetSize / 4u - 1u));
    insert4Byte(remb, senderSsrc);
    insert4Byte(remb, 0x0);              // ssrc source
    insert4Byte(remb, uniqueIdentifier); // uid
    remb.insert(remb.end(), static_cast<uint8_t>(feedbackSsrcs.size()));

    const uint32_t maxMantissa = 0x3ffff; // 18 bits.
    uint64_t mantissa = bitrate_bps;
    uint8_t exponent = 0;
    while (mantissa > maxMantissa) {
        mantissa >>= 1;
        ++exponent;
    }

    remb.insert(remb.end(), (exponent << 2) | (mantissa >> 16));
    insert2Byte(remb, mantissa & 0xffff);
    for (auto ssrc : feedbackSsrcs)
        insert4Byte(remb, ssrc);

    return remb;
}

std::optional<TransportCcBitrateEstimate>
CongestionControl::estimateTransportCcBitrate(uint64_t currentBitrateBps, const std::list<TransportCcReport>& reports)
{
    size_t totalPackets = 0;
    size_t receivedPackets = 0;
    size_t lostPackets = 0;
    uint64_t receivedBytes = 0;
    int64_t firstReceiveTimeUs = 0;
    int64_t lastReceiveTimeUs = 0;
    int64_t previousSendTimeUs = 0;
    int64_t previousReceiveTimeUs = 0;
    int64_t delayTrendUs = 0;
    bool hasReceiveTime = false;
    bool hasPreviousReceivedPacket = false;

    for (const auto& report : reports) {
        for (const auto& packet : report.packets) {
            ++totalPackets;
            if (packet.status == TransportCcPacketStatus::NotReceived) {
                ++lostPackets;
                continue;
            }

            ++receivedPackets;
            receivedBytes += packet.payloadSize;
            if (!hasReceiveTime) {
                firstReceiveTimeUs = packet.receiveTimeOffsetUs;
                hasReceiveTime = true;
            }
            lastReceiveTimeUs = packet.receiveTimeOffsetUs;

            if (hasPreviousReceivedPacket) {
                delayTrendUs += (packet.receiveTimeOffsetUs - previousReceiveTimeUs)
                                - (packet.sendTimeOffsetUs - previousSendTimeUs);
            }
            previousSendTimeUs = packet.sendTimeOffsetUs;
            previousReceiveTimeUs = packet.receiveTimeOffsetUs;
            hasPreviousReceivedPacket = true;
        }
    }

    if (totalPackets == 0)
        return std::nullopt;

    const auto packetLoss = static_cast<float>(lostPackets * 100) / static_cast<float>(totalPackets);
    uint64_t observedBitrateBps = 0;
    const auto receiveSpanUs = lastReceiveTimeUs - firstReceiveTimeUs;
    if (receivedPackets > 1 && receiveSpanUs > 0) {
        observedBitrateBps = static_cast<uint64_t>((receivedBytes * 8 * 1000000ULL)
                                                   / static_cast<uint64_t>(receiveSpanUs));
    }

    auto targetBitrateBps = currentBitrateBps ? currentBitrateBps : observedBitrateBps;
    if (targetBitrateBps == 0)
        return std::nullopt;

    static constexpr float LOSS_CONGESTION_THRESHOLD = 2.0f;
    static constexpr int64_t DELAY_CONGESTION_THRESHOLD_US = 15000;
    if (packetLoss > LOSS_CONGESTION_THRESHOLD || delayTrendUs > DELAY_CONGESTION_THRESHOLD_US) {
        auto decreaseRatio = packetLoss > LOSS_CONGESTION_THRESHOLD ? 1.0 - std::min<double>(packetLoss / 100.0, 0.5)
                                                                    : 0.85;
        targetBitrateBps = static_cast<uint64_t>(static_cast<double>(targetBitrateBps) * decreaseRatio);
        if (observedBitrateBps)
            targetBitrateBps = std::min<uint64_t>(targetBitrateBps, observedBitrateBps * 9 / 10);
    } else if (observedBitrateBps > targetBitrateBps) {
        targetBitrateBps = std::min<uint64_t>(targetBitrateBps + targetBitrateBps / 12, observedBitrateBps * 11 / 10);
    }

    TransportCcBitrateEstimate estimate;
    estimate.bitrateBps = targetBitrateBps;
    estimate.packetLoss = packetLoss;
    estimate.receivedPackets = receivedPackets;
    estimate.lostPackets = lostPackets;
    estimate.delayTrendUs = delayTrendUs;
    return estimate;
}

float
CongestionControl::kalmanFilter(int gradiant_delay)
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
    return ((1 - k) * last_estimate_m_) + (k * static_cast<float>(d_m));
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
    return ((1 - k) * (last_var_p_ + q));
}

float
CongestionControl::get_var_n(int d_m)
{
    float z = get_residual_z(d_m);
    // JAMI_WARN("var_n: %f", (beta * last_var_n_) + ((1.0f - beta) * z * z));
    return (beta * last_var_n_) + ((1.0f - beta) * z * z);
}

float
CongestionControl::get_residual_z(int d_m)
{
    // JAMI_WARN("z: %f", d_m - last_estimate_m_);
    return (static_cast<float>(d_m) - last_estimate_m_);
}

float
CongestionControl::update_thresh(float m, int deltaT)
{
    float ky = 0.0f;
    if (std::fabs(m) < last_thresh_y_)
        ky = kd;
    else
        ky = ku;
    float res = last_thresh_y_ + ((static_cast<float>(deltaT) * ky) * (std::fabs(m) - last_thresh_y_));
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
    if (estimation > thresh) {
        // JAMI_WARN("Enter overuse state");
        if (not overuse_counter_) {
            t0_overuse = clock::now();
            overuse_counter_++;
            return bwNormal;
        }
        overuse_counter_++;
        time_point now = clock::now();
        auto overuse_timer = now - t0_overuse;
        if ((overuse_timer >= OVERUSE_THRESH) and (overuse_counter_ > 1)) {
            overuse_counter_ = 0;
            last_state_ = bwOverusing;
        }
    } else if (estimation < -thresh) {
        // JAMI_WARN("Enter underuse state");
        overuse_counter_ = 0;
        last_state_ = bwUnderusing;
    } else {
        overuse_counter_ = 0;
        last_state_ = bwNormal;
    }
    return last_state_;
}

} // namespace jami