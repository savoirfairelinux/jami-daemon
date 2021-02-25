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

#ifndef MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_REMB_H_
#define MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_REMB_H_

#include <vector>
#include <cstdint>
#include <memory>

#include "socket_pair.h"

namespace jami {

enum BandwidthUsage { bwNormal = 0, bwUnderusing = 1, bwOverusing = 2 };

class BwEstimator;

using clock = std::chrono::steady_clock;
using time_point = clock::time_point;

// Receiver Estimated Max Bitrate (REMB) (draft-alvestrand-rmcat-remb).
class CongestionControl
{
public:
    CongestionControl(bool useTrendline = false);
    ~CongestionControl();

    uint64_t parseREMB(const rtcpREMBHeader& packet);
    std::vector<uint8_t> createREMB(uint64_t bitrate_bps);
    BandwidthUsage get_bw_state(float estimation, float thresh);
    std::unique_ptr<BwEstimator>& getEstimator() { return estimator_; }
    BandwidthUsage getStateBW();
    void update(double recv_delta_ms,
                double send_delta_ms,
                time_point arrival_time_ms);

private:
    std::unique_ptr<BwEstimator> estimator_;
};
class BwEstimator
{
public:
    virtual ~BwEstimator();

    virtual void update(double recv_delta_ms,
                double send_delta_ms,
                time_point arrival_time_ms) = 0;
    virtual BandwidthUsage getStateBW() = 0;
};

class KalmanEstimator : public BwEstimator
{
public:
    KalmanEstimator();
    ~KalmanEstimator();

    void update(double recv_delta_ms,
                double send_delta_ms,
                time_point arrival_time_ms);
    BandwidthUsage getStateBW() { return last_state_; }
    void detect(float estimation);

private:
    static constexpr float Q {0.5f};
    static constexpr float beta {0.95f};
    static constexpr float ku {0.004f};
    static constexpr float kd {0.002f};
    static constexpr std::chrono::duration OVERUSE_THRESH {std::chrono::milliseconds(100)};


    float get_estimate_m(float k, int d_m);
    float get_gain_k(float q, float dev_n);
    float get_sys_var_p(float k, float q);
    float get_var_n(int d_m);
    float get_residual_z(float d_m);
    void updateThreshold(float m, int deltaR);

    float last_estimate_m_ {0.0f};
    float last_var_p_ {0.1f};
    float last_var_n_ {0.0f};
    float last_thresh_y_ {2.0f};
    BandwidthUsage last_state_;
    unsigned overuse_counter_;
    time_point t0_overuse {time_point::min()};
};

class TrendlineEstimator : public BwEstimator
{
public:
    struct PacketTiming {
        PacketTiming(double arrival_time_ms,
                    double smoothed_delay_ms,
                    double raw_delay_ms)
            : arrival_time_ms(arrival_time_ms),
            smoothed_delay_ms(smoothed_delay_ms),
            raw_delay_ms(raw_delay_ms) {}
        double arrival_time_ms;
        double smoothed_delay_ms;
        double raw_delay_ms;
    };

    struct Settings {
        static constexpr unsigned kDefaultTrendlineWindowSize = 20;

        // Sort the packets in the window. Should be redundant,
        // but then almost no cost.
        bool enable_sort = false;

        // Cap the trendline slope based on the minimum delay seen
        // in the beginning_packets and end_packets respectively.
        bool enable_cap = false;
        unsigned beginning_packets = 7;
        unsigned end_packets = 7;
        double cap_uncertainty = 0.0;

        // Size (in packets) of the window.
        unsigned window_size = kDefaultTrendlineWindowSize;
    };

public:
    TrendlineEstimator();
    ~TrendlineEstimator();

    void update(double recv_delta_ms,
                double send_delta_ms,
                time_point arrival_time_ms);
    BandwidthUsage getStateBW() { return hypothesis_; }

private:
    void detect(double trend, double ts_delta, time_point now);
    void UpdateThreshold(double modified_offset);
    double linearFitSlope(
        const std::deque<TrendlineEstimator::PacketTiming>& packets);
    double computeSlopeCap(
        const std::deque<TrendlineEstimator::PacketTiming>& packets,
        const TrendlineEstimator::Settings& settings);
    void updateThreshold(double modified_trend, time_point now);
    // Parameters.
    Settings settings_;
    const double smoothing_coef_;
    const double threshold_gain_;
    // Used by the existing threshold.
    int num_of_deltas_;
    // Keep the arrival times small by using the change from the first packet.
    time_point first_arrival_time_;
    // Exponential backoff filtering.
    double accumulated_delay_;
    double smoothed_delay_;
    // Linear least squares regression.
    std::deque<PacketTiming> delay_hist_;

    const double k_up_;
    const double k_down_;
    double overusing_time_threshold_;
    double threshold_;
    double prev_modified_trend_;
    time_point last_update_;
    double prev_trend_;
    double time_over_using_;
    int overuse_counter_;
    BandwidthUsage hypothesis_;

    // constexpr
    static constexpr double kMaxAdaptOffsetMs = 15.0;
    static constexpr double kOverUsingTimeThreshold = 10;
    static constexpr int kMinNumDeltas = 60;
    static constexpr int kDeltaCounterMax = 1000;
    static constexpr double kDefaultTrendlineSmoothingCoeff = 0.9;
    static constexpr double kDefaultTrendlineThresholdGain = 4.0;

};

} // namespace jami
#endif
