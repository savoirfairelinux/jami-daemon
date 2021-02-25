/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
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
#include <algorithm>

namespace jami {
static constexpr uint8_t packetVersion = 2;
static constexpr uint8_t packetFMT = 15;
static constexpr uint8_t packetType = 206;
static constexpr uint32_t uniqueIdentifier = 0x52454D42; // 'R' 'E' 'M' 'B'.

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

CongestionControl::CongestionControl(bool useTrendline)
{
    if (useTrendline)
        estimator_ = std::make_unique<TrendlineEstimator>();
    else
        estimator_ = std::make_unique<KalmanEstimator>();
}

CongestionControl::~CongestionControl() {}


uint64_t
CongestionControl::parseREMB(const rtcpREMBHeader& packet)
{
    if (packet.fmt != 15 || packet.pt != 206) {
        JAMI_ERR("Unable to parse REMB packet.");
        return 0;
    }
    uint64_t bitrate_bps = (packet.br_mantis << packet.br_exp);
    bool shift_overflow = (bitrate_bps >> packet.br_exp) != packet.br_mantis;
    if (shift_overflow) {
        JAMI_ERR("Invalid remb bitrate value : %u*2^%u", packet.br_mantis, packet.br_exp);
        return false;
    }
    return bitrate_bps;
}

std::vector<uint8_t>
CongestionControl::createREMB(uint64_t bitrate_bps)
{
    std::vector<uint8_t> remb;
    remb.reserve(24);

    remb.insert(remb.end(), packetVersion << 6 | packetFMT);
    remb.insert(remb.end(), packetType);
    insert2Byte(remb, 5);                // (sizeof(rtcpREMBHeader)/4)-1 -> not safe
    insert4Byte(remb, 0x12345678);       // ssrc
    insert4Byte(remb, 0x0);              // ssrc source
    insert4Byte(remb, uniqueIdentifier); // uid
    remb.insert(remb.end(), 1);          // n_ssrc

    const uint32_t maxMantissa = 0x3ffff; // 18 bits.
    uint64_t mantissa = bitrate_bps;
    uint8_t exponenta = 0;
    while (mantissa > maxMantissa) {
        mantissa >>= 1;
        ++exponenta;
    }

    remb.insert(remb.end(), (exponenta << 2) | (mantissa >> 16));
    insert2Byte(remb, mantissa & 0xffff);
    insert4Byte(remb, 0x2345678b);

    return remb;
}

BandwidthUsage
CongestionControl::getStateBW()
{
    return getEstimator()->getStateBW();
}

void
CongestionControl::update(double recv_delta_ms,
                double send_delta_ms,
                time_point arrival_time_ms)
{
    getEstimator()->update(recv_delta_ms, send_delta_ms, arrival_time_ms);
}


BwEstimator::~BwEstimator()
{}

KalmanEstimator::KalmanEstimator()
{}

KalmanEstimator::~KalmanEstimator()
{}

void
KalmanEstimator::update(double recv_delta_ms,
                double send_delta_ms,
                time_point arrival_time_ms)
{
    int gradiant_delay = recv_delta_ms - send_delta_ms;
    float var_n = get_var_n(gradiant_delay);
    float k = get_gain_k(Q, var_n);
    float m = get_estimate_m(k, gradiant_delay);
    last_var_p_ = get_sys_var_p(k, Q);
    last_estimate_m_ = m;
    last_var_n_ = var_n;

    detect(m);
    updateThreshold(m, recv_delta_ms);
}

float
KalmanEstimator::get_estimate_m(float k, int d_m)
{
    // JAMI_WARN("[get_estimate_m]k:%f, last_estimate_m_:%f, d_m:%f", k, last_estimate_m_, d_m);
    // JAMI_WARN("m: %f", ((1-k) * last_estimate_m_) + (k * d_m));
    return ((1 - k) * last_estimate_m_) + (k * d_m);
}

float
KalmanEstimator::get_gain_k(float q, float dev_n)
{
    // JAMI_WARN("k: %f", (last_var_p_ + q) / (last_var_p_ + q + dev_n));
    return (last_var_p_ + q) / (last_var_p_ + q + dev_n);
}

float
KalmanEstimator::get_sys_var_p(float k, float q)
{
    // JAMI_WARN("var_p: %f", ((1-k) * (last_var_p_ + q)));
    return ((1 - k) * (last_var_p_ + q));
}

float
KalmanEstimator::get_var_n(int d_m)
{
    float z = get_residual_z(d_m);
    // JAMI_WARN("var_n: %f", (beta * last_var_n_) + ((1.0f - beta) * z * z));
    return (beta * last_var_n_) + ((1.0f - beta) * z * z);
}

float
KalmanEstimator::get_residual_z(float d_m)
{
    // JAMI_WARN("z: %f", d_m - last_estimate_m_);
    return (d_m - last_estimate_m_);
}

void
KalmanEstimator::detect(float estimation)
{
    if (estimation > last_thresh_y_) {
        // JAMI_WARN("Enter overuse state");
        if (not overuse_counter_) {
            t0_overuse = clock::now();
            overuse_counter_++;
            last_state_ = bwNormal;
            return;
        }
        overuse_counter_++;
        time_point now = clock::now();
        auto overuse_timer = now - t0_overuse;
        if ((overuse_timer >= OVERUSE_THRESH) and (overuse_counter_ > 1)) {
            overuse_counter_ = 0;
            last_state_ = bwOverusing;
            return;
        }
    } else if (estimation < -last_thresh_y_) {
        // JAMI_WARN("Enter underuse state");
        overuse_counter_ = 0;
        last_state_ = bwUnderusing;
        return;
    } else {
        overuse_counter_ = 0;
        last_state_ = bwNormal;
        return;
    }
}

void
KalmanEstimator::updateThreshold(float m, int deltaR)
{
    float ky = 0.0f;
    if (std::fabs(m) < last_thresh_y_)
        ky = kd;
    else
        ky = ku;
    last_thresh_y_ = last_thresh_y_ + ((deltaR * ky) * (std::fabs(m) - last_thresh_y_));
}


TrendlineEstimator::TrendlineEstimator()
    : smoothing_coef_(kDefaultTrendlineSmoothingCoeff),
      threshold_gain_(kDefaultTrendlineThresholdGain),
      num_of_deltas_(0),
      first_arrival_time_(time_point::min()),
      accumulated_delay_(0),
      smoothed_delay_(0),
      delay_hist_(),
      k_up_(0.0087),
      k_down_(0.039),
      overusing_time_threshold_(kOverUsingTimeThreshold),
      threshold_(12.5),
      prev_modified_trend_(NAN),
      last_update_(time_point::min()),
      prev_trend_(0.0),
      time_over_using_(-1),
      overuse_counter_(0),
      hypothesis_(BandwidthUsage::bwNormal)
{}

TrendlineEstimator::~TrendlineEstimator()
{}

void
TrendlineEstimator::update(double recv_delta_ms,
                            double send_delta_ms,
                            time_point arrival_time_ms)
{
    const double delta_ms = recv_delta_ms - send_delta_ms;
    ++num_of_deltas_;
    num_of_deltas_ = std::min(num_of_deltas_, kDeltaCounterMax);
    if (first_arrival_time_ == time_point::min())
        first_arrival_time_ = arrival_time_ms;

    // Exponential backoff filter.
    accumulated_delay_ += delta_ms;
    smoothed_delay_ = smoothing_coef_ * smoothed_delay_ +
                    (1 - smoothing_coef_) * accumulated_delay_;

    // Maintain packet window
    delay_hist_.emplace_back(
        std::chrono::duration_cast<std::chrono::milliseconds>(arrival_time_ms - first_arrival_time_).count(),
        smoothed_delay_,
        accumulated_delay_);
    if (delay_hist_.size() > settings_.window_size)
        delay_hist_.pop_front();

    // Simple linear regression.
    double trend = prev_trend_;
    if (delay_hist_.size() == settings_.window_size) {
        // Update trend_ if it is possible to fit a line to the data. The delay
        // trend can be seen as an estimate of (send_rate - capacity)/capacity.
        // 0 < trend < 1   ->  the delay increases, queues are filling up
        //   trend == 0    ->  the delay does not change
        //   trend < 0     ->  the delay decreases, queues are being emptied
        trend = linearFitSlope(delay_hist_);
        if (trend == 0.0f)
            trend = prev_trend_;
        if (settings_.enable_cap) {
            double cap = computeSlopeCap(delay_hist_, settings_);
            // We only use the cap to filter out overuse detections, not
            // to detect additional underuses.
            if (trend >= 0 && cap != 0.0f && trend > cap) {
                trend = cap;
            }
        }
    }

    detect(trend, send_delta_ms, arrival_time_ms);
}

double
TrendlineEstimator::linearFitSlope(
    const std::deque<TrendlineEstimator::PacketTiming>& packets)
{
    // Compute the "center of mass".
    double sum_x = 0;
    double sum_y = 0;
    for (const auto& packet : packets) {
        sum_x += packet.arrival_time_ms;
        sum_y += packet.smoothed_delay_ms;
    }
    double x_avg = sum_x / packets.size();
    double y_avg = sum_y / packets.size();
    // Compute the slope k = \sum (x_i-x_avg)(y_i-y_avg) / \sum (x_i-x_avg)^2
    double numerator = 0;
    double denominator = 0;
    for (const auto& packet : packets) {
        double x = packet.arrival_time_ms;
        double y = packet.smoothed_delay_ms;
        numerator += (x - x_avg) * (y - y_avg);
        denominator += (x - x_avg) * (x - x_avg);
    }
    if (denominator == 0)
        return 0.0f;
    return numerator / denominator;
}

double
TrendlineEstimator::computeSlopeCap(
    const std::deque<TrendlineEstimator::PacketTiming>& packets,
    const TrendlineEstimator::Settings& settings)
{
    TrendlineEstimator::PacketTiming early = packets[0];
    for (size_t i = 1; i < settings.beginning_packets; ++i) {
        if (packets[i].raw_delay_ms < early.raw_delay_ms)
            early = packets[i];
    }
    size_t late_start = packets.size() - settings.end_packets;
    TrendlineEstimator::PacketTiming late = packets[late_start];
    for (size_t i = late_start + 1; i < packets.size(); ++i) {
        if (packets[i].raw_delay_ms < late.raw_delay_ms)
            late = packets[i];
    }
    if (late.arrival_time_ms - early.arrival_time_ms < 1) {
        return 0.0f;
    }
    return (late.raw_delay_ms - early.raw_delay_ms) /
            (late.arrival_time_ms - early.arrival_time_ms) +
            settings.cap_uncertainty;
}

void
TrendlineEstimator::detect(double trend, double ts_delta, time_point now)
{
    if (num_of_deltas_ < 2) {
        hypothesis_ = bwNormal;
        return;
    }
    const double modified_trend =
        std::min(num_of_deltas_, kMinNumDeltas) * trend * threshold_gain_;
    prev_modified_trend_ = modified_trend;
    if (modified_trend > threshold_) {
        if (time_over_using_ == -1) {
            // Initialize the timer. Assume that we've been
            // over-using half of the time since the previous
            // sample.
            time_over_using_ = ts_delta / 2;
        } else {
            // Increment timer
            time_over_using_ += ts_delta;
        }
        overuse_counter_++;
        if (time_over_using_ > overusing_time_threshold_ && overuse_counter_ > 1) {
            if (trend >= prev_trend_) {
                time_over_using_ = 0;
                overuse_counter_ = 0;
                hypothesis_ = bwOverusing;
            }
        }
    } else if (modified_trend < -threshold_) {
        time_over_using_ = -1;
        overuse_counter_ = 0;
        hypothesis_ = bwUnderusing;
    } else {
        time_over_using_ = -1;
        overuse_counter_ = 0;
        hypothesis_ = bwNormal;
    }
    prev_trend_ = trend;
    updateThreshold(modified_trend, now);
}

void
TrendlineEstimator::updateThreshold(double modified_trend,
                                         time_point now)
{
    if (last_update_ == time_point::min())
        last_update_ = now;

    if (fabs(modified_trend) > threshold_ + kMaxAdaptOffsetMs) {
        // Avoid adapting the threshold to big latency spikes, caused e.g.,
        // by a sudden capacity drop.
        last_update_ = now;
        return;
    }

    const double k = fabs(modified_trend) < threshold_ ? k_down_ : k_up_;
    const int64_t kMaxTimeDeltaMs = 100;
    int64_t time_delta_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update_)
                      .count();
    time_delta_ms = std::min(time_delta_ms, kMaxTimeDeltaMs);
    threshold_ += k * (fabs(modified_trend) - threshold_) * time_delta_ms;
    threshold_ = std::clamp(threshold_, 6.0, 600.0);
    last_update_ = now;
}

} // namespace jami