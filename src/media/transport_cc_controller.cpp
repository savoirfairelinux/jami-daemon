#include "transport_cc_controller.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace jami {
namespace {

struct TransportCcWindowStats
{
    size_t totalPackets {};
    size_t receivedPackets {};
    size_t lostPackets {};
    uint64_t sentBytes {};
    uint64_t receivedBytes {};
    int64_t firstSendTimeUs {};
    int64_t lastSendTimeUs {};
    int64_t firstReceiveTimeUs {};
    int64_t lastReceiveTimeUs {};
    int64_t accumulatedDelayTrendUs {};
    size_t delayTrendSamples {};
    size_t maxConsecutiveLostPackets {};
    size_t missingSendHistoryPackets {};
    bool hasSendTime {false};
    bool hasReceiveTime {false};
    bool hasFeedbackReceiveTime {false};
    int64_t latestFeedbackReceiveTimeUs {};
    std::vector<std::pair<int64_t, int64_t>> delaySamples;
    std::vector<std::pair<int64_t, size_t>> receivedSamples;
    std::vector<uint8_t> feedbackPacketCounts;
    std::vector<uint16_t> feedbackMaxSequenceNumbers;
};

struct DelayTrendGroup
{
    int64_t firstReceiveTimeUs {};
    int64_t lastReceiveTimeUs {};
    int64_t accumulatedDelayDeltaUs {};
    size_t samples {};
    bool active {false};
};

void
flushDelayTrendGroup(TransportCcWindowStats& stats, DelayTrendGroup& group)
{
    if (!group.active || group.samples == 0)
        return;

    stats.accumulatedDelayTrendUs += group.accumulatedDelayDeltaUs;
    ++stats.delayTrendSamples;
    stats.delaySamples.emplace_back(group.lastReceiveTimeUs, group.accumulatedDelayDeltaUs);
    group = {};
}

void
addDelayTrendDelta(TransportCcWindowStats& stats,
                   DelayTrendGroup& group,
                   int64_t receiveTimeUs,
                   int64_t delayDeltaUs,
                   const TransportCcControllerConfig& config)
{
    const auto absoluteDelayDeltaUs = delayDeltaUs < 0 ? -delayDeltaUs : delayDeltaUs;
    if (absoluteDelayDeltaUs > config.delayTrendOutlierThresholdUs)
        return;

    const auto groupLengthUs = std::max<int64_t>(1, config.delayTrendGroupLengthUs);
    if (group.active && receiveTimeUs - group.firstReceiveTimeUs > groupLengthUs)
        flushDelayTrendGroup(stats, group);

    if (!group.active) {
        group.firstReceiveTimeUs = receiveTimeUs;
        group.active = true;
    }

    group.lastReceiveTimeUs = receiveTimeUs;
    group.accumulatedDelayDeltaUs += delayDeltaUs;
    ++group.samples;
}

bool
sequenceNumberIsNewer(uint16_t sequenceNumber, uint16_t previousSequenceNumber)
{
    return sequenceNumber != previousSequenceNumber
           && static_cast<int16_t>(sequenceNumber - previousSequenceNumber) > 0;
}

TransportCcWindowStats
collectStats(const std::list<TransportCcReport>& reports, const TransportCcControllerConfig& config)
{
    TransportCcWindowStats stats;
    DelayTrendGroup delayGroup;
    for (const auto& report : reports) {
        stats.feedbackPacketCounts.push_back(report.feedback.feedbackPacketCount);
        stats.missingSendHistoryPackets += report.missingSendHistoryPackets;
        if (report.feedbackReceiveTimeUs != 0) {
            stats.latestFeedbackReceiveTimeUs = stats.hasFeedbackReceiveTime
                                                    ? std::max(stats.latestFeedbackReceiveTimeUs,
                                                               report.feedbackReceiveTimeUs)
                                                    : report.feedbackReceiveTimeUs;
            stats.hasFeedbackReceiveTime = true;
        }
        std::optional<uint16_t> reportMaxSequenceNumber;
        int64_t previousSendTimeUs = 0;
        int64_t previousReceiveTimeUs = 0;
        size_t consecutiveLostPackets = 0;
        bool hasPreviousReceivedPacket = false;
        for (const auto& packet : report.packets) {
            if (!reportMaxSequenceNumber || sequenceNumberIsNewer(packet.sequenceNumber, *reportMaxSequenceNumber))
                reportMaxSequenceNumber = packet.sequenceNumber;

            ++stats.totalPackets;
            stats.sentBytes += packet.payloadSize;
            if (!stats.hasSendTime) {
                stats.firstSendTimeUs = packet.sendTimeOffsetUs;
                stats.lastSendTimeUs = packet.sendTimeOffsetUs;
                stats.hasSendTime = true;
            } else {
                stats.firstSendTimeUs = std::min(stats.firstSendTimeUs, packet.sendTimeOffsetUs);
                stats.lastSendTimeUs = std::max(stats.lastSendTimeUs, packet.sendTimeOffsetUs);
            }

            if (packet.status == TransportCcPacketStatus::NotReceived) {
                ++stats.lostPackets;
                ++consecutiveLostPackets;
                stats.maxConsecutiveLostPackets = std::max(stats.maxConsecutiveLostPackets, consecutiveLostPackets);
                continue;
            }

            consecutiveLostPackets = 0;
            ++stats.receivedPackets;
            stats.receivedBytes += packet.payloadSize;
            stats.receivedSamples.emplace_back(packet.receiveTimeOffsetUs, packet.payloadSize);
            if (!stats.hasReceiveTime) {
                stats.firstReceiveTimeUs = packet.receiveTimeOffsetUs;
                stats.lastReceiveTimeUs = packet.receiveTimeOffsetUs;
                stats.hasReceiveTime = true;
            } else {
                stats.firstReceiveTimeUs = std::min(stats.firstReceiveTimeUs, packet.receiveTimeOffsetUs);
                stats.lastReceiveTimeUs = std::max(stats.lastReceiveTimeUs, packet.receiveTimeOffsetUs);
            }

            if (hasPreviousReceivedPacket) {
                const auto delayDeltaUs = (packet.receiveTimeOffsetUs - previousReceiveTimeUs)
                                          - (packet.sendTimeOffsetUs - previousSendTimeUs);
                addDelayTrendDelta(stats, delayGroup, packet.receiveTimeOffsetUs, delayDeltaUs, config);
            }
            previousSendTimeUs = packet.sendTimeOffsetUs;
            previousReceiveTimeUs = packet.receiveTimeOffsetUs;
            hasPreviousReceivedPacket = true;
        }
        if (reportMaxSequenceNumber)
            stats.feedbackMaxSequenceNumbers.push_back(*reportMaxSequenceNumber);
    }
    flushDelayTrendGroup(stats, delayGroup);
    return stats;
}

uint64_t
estimateSentBitrateBps(const TransportCcWindowStats& stats)
{
    const auto sendSpanUs = stats.lastSendTimeUs - stats.firstSendTimeUs;
    if (stats.totalPackets <= 1 || sendSpanUs <= 0)
        return 0;
    return static_cast<uint64_t>((stats.sentBytes * 8 * 1'000'000ULL) / static_cast<uint64_t>(sendSpanUs));
}

uint64_t
estimateAcknowledgedBitrateBps(const TransportCcWindowStats& stats)
{
    const auto receiveSpanUs = stats.lastReceiveTimeUs - stats.firstReceiveTimeUs;
    if (stats.receivedPackets <= 1 || receiveSpanUs <= 0)
        return 0;
    return static_cast<uint64_t>((stats.receivedBytes * 8 * 1'000'000ULL) / static_cast<uint64_t>(receiveSpanUs));
}

} // namespace

TransportCcController::TransportCcController(TransportCcControllerConfig config)
    : config_(config)
{}

std::optional<TransportCcControllerEstimate>
TransportCcController::update(uint64_t currentBitrateBps, const std::list<TransportCcReport>& reports)
{
    resetOnFeedbackMediaSsrcChange(reports);

    const auto stats = collectStats(reports, config_);
    if (stats.totalPackets == 0)
        return std::nullopt;

    for (const auto& [receiveTimeUs, payloadSize] : stats.receivedSamples)
        addAcknowledgedBitrateSample(receiveTimeUs, payloadSize);
    const auto acknowledgedBitrateBps = this->acknowledgedBitrateBps().value_or(estimateAcknowledgedBitrateBps(stats));
    auto workingBitrateBps = currentBitrateBps ? currentBitrateBps : targetBitrateBps_;
    if (workingBitrateBps == 0)
        workingBitrateBps = acknowledgedBitrateBps;
    if (workingBitrateBps == 0)
        return std::nullopt;

    TransportCcControllerEstimate estimate;
    estimate.acknowledgedBitrateBps = acknowledgedBitrateBps;
    estimate.sentBitrateBps = estimateSentBitrateBps(stats);
    const auto rawPacketLossRatio = static_cast<double>(stats.lostPackets) / static_cast<double>(stats.totalPackets);
    for (const auto& [receiveTimeUs, delayDeltaUs] : stats.delaySamples)
        addDelayTrendSample(receiveTimeUs, delayDeltaUs);
    const auto averageDelayTrendUs = stats.delayTrendSamples ? static_cast<double>(stats.accumulatedDelayTrendUs)
                                                                   / static_cast<double>(stats.delayTrendSamples)
                                                             : 0.0;
    const auto rawDelayTrendUs = delayTrendlineUs().value_or(averageDelayTrendUs);
    smoothedLossRatio_ = hasSmoothedLossRatio_
                             ? smooth(smoothedLossRatio_, rawPacketLossRatio, config_.lossSmoothingFactor)
                             : rawPacketLossRatio;
    smoothedDelayTrendUs_ = hasSmoothedDelayTrend_
                                ? smooth(smoothedDelayTrendUs_, rawDelayTrendUs, config_.delayTrendSmoothingFactor)
                                : rawDelayTrendUs;
    hasSmoothedLossRatio_ = true;
    hasSmoothedDelayTrend_ = true;

    estimate.packetLossRatio = smoothedLossRatio_;
    estimate.receivedPackets = stats.receivedPackets;
    estimate.lostPackets = stats.lostPackets;
    estimate.delayTrendUs = static_cast<int64_t>(std::lround(smoothedDelayTrendUs_));
    estimate.queueDelayUs = hasMinAccumulatedDelay_
                                ? static_cast<int64_t>(std::lround(accumulatedDelayUs_ - minAccumulatedDelayUs_))
                                : 0;
    estimate.maxConsecutiveLostPackets = stats.maxConsecutiveLostPackets;
    estimate.missingSendHistoryPackets = stats.missingSendHistoryPackets;
    updateFeedbackState(stats.feedbackPacketCounts, estimate);
    updateFeedbackSequenceState(stats.feedbackMaxSequenceNumbers, estimate);
    if (stats.hasFeedbackReceiveTime) {
        const auto nowUs = std::chrono::duration_cast<std::chrono::microseconds>(
                               std::chrono::steady_clock::now().time_since_epoch())
                               .count();
        estimate.feedbackAgeUs = std::max<int64_t>(0, nowUs - stats.latestFeedbackReceiveTimeUs);
    }
    estimate.confidence = estimateConfidence(stats.totalPackets, acknowledgedBitrateBps, estimate);

    const auto isSenderLimited = estimate.sentBitrateBps != 0
                                 && estimate.sentBitrateBps < static_cast<uint64_t>(
                                        static_cast<double>(workingBitrateBps) * config_.increaseUtilizationThreshold);
    if (isSenderLimited)
        estimate.confidence *= std::clamp(1.0 - config_.senderLimitedConfidencePenalty, 0.0, 1.0);

    const auto hasDelayTrendOveruse = estimate.delayTrendUs > config_.delayTrendThresholdUs;
    const auto hasQueueOveruse = estimate.queueDelayUs >= config_.congestionQueueDelayThresholdUs
                                 && estimate.delayTrendUs > 0;
    const auto hasDelayOveruse = hasDelayTrendOveruse || hasQueueOveruse;
    delayOveruseCount_ = hasDelayOveruse ? delayOveruseCount_ + 1 : 0;
    const auto hasPersistentDelayOveruse = delayOveruseCount_ >= config_.delayOveruseCountThreshold;
    estimate.lossType = classifyLoss(estimate, rawPacketLossRatio, hasDelayOveruse || hasPersistentDelayOveruse);
    const auto sendDurationUs = stats.hasSendTime ? stats.lastSendTimeUs - stats.firstSendTimeUs : 0;
    validatePendingProbeCluster(estimate,
                                stats.totalPackets,
                                sendDurationUs,
                                hasDelayOveruse || hasPersistentDelayOveruse);
    const auto hasCongestionLossOveruse = estimate.lossType == TransportCcLossType::Congestion;
    const auto hasRandomLossOveruse = estimate.lossType == TransportCcLossType::Random
                                      && estimate.packetLossRatio >= config_.randomLossDecreaseThresholdRatio;
    const auto hasBurstLossOveruse = estimate.lossType == TransportCcLossType::Burst
                                     && estimate.packetLossRatio >= config_.burstLossDecreaseThresholdRatio;
    const auto hasLossOveruse = rawPacketLossRatio > config_.lossThresholdRatio
                                && estimate.packetLossRatio > config_.lossThresholdRatio
                                && (hasCongestionLossOveruse || hasRandomLossOveruse || hasBurstLossOveruse);
    const auto isNearlyFullyUtilized = acknowledgedBitrateBps
                                       >= static_cast<uint64_t>(static_cast<double>(workingBitrateBps)
                                                                * config_.increaseUtilizationThreshold);
    const auto isAppLimited = isSenderLimited
                              || (estimate.sentBitrateBps == 0 && acknowledgedBitrateBps != 0 && !isNearlyFullyUtilized);
    const auto isUnderusing = estimate.delayTrendUs < config_.underuseTrendThresholdUs;
    const auto hasConfidenceToIncrease = estimate.confidence >= config_.minConfidenceForIncrease;
    const auto hasCleanRecoveryFeedback = recoveringFromOveruse_ && !hasDelayOveruse
                                          && estimate.packetLossRatio <= config_.lossThresholdRatio
                                          && isNearlyFullyUtilized && hasConfidenceToIncrease;
    if (hasLossOveruse || hasPersistentDelayOveruse || !recoveringFromOveruse_ || isAppLimited)
        cleanRecoveryReports_ = 0;
    else if (hasCleanRecoveryFeedback)
        ++cleanRecoveryReports_;

    if (hasLossOveruse) {
        const auto decreaseRatio = std::max(config_.lossDecreaseFloor, 1.0 - estimate.packetLossRatio);
        estimate.targetBitrateBps = static_cast<uint64_t>(
            std::lround(static_cast<double>(workingBitrateBps) * decreaseRatio));
        if (acknowledgedBitrateBps)
            estimate.targetBitrateBps = std::min<uint64_t>(estimate.targetBitrateBps, acknowledgedBitrateBps);
        estimate.state = TransportCcBandwidthState::Decrease;
        estimate.reason = TransportCcEstimateReason::PacketLoss;
        increaseCooldownReports_ = config_.increaseCooldownAfterDecreaseReports;
    } else if (hasPersistentDelayOveruse) {
        estimate.targetBitrateBps = static_cast<uint64_t>(
            std::lround(static_cast<double>(workingBitrateBps) * config_.delayDecreaseRatio));
        if (acknowledgedBitrateBps)
            estimate.targetBitrateBps = std::min<uint64_t>(estimate.targetBitrateBps, acknowledgedBitrateBps);
        estimate.state = TransportCcBandwidthState::Decrease;
        estimate.reason = TransportCcEstimateReason::DelayIncrease;
        increaseCooldownReports_ = config_.increaseCooldownAfterDecreaseReports;
    } else if (!hasDelayOveruse && estimate.packetLossRatio <= config_.lossThresholdRatio && isNearlyFullyUtilized
               && hasConfidenceToIncrease) {
        if (increaseCooldownReports_ != 0) {
            --increaseCooldownReports_;
            estimate.targetBitrateBps = workingBitrateBps;
            estimate.state = TransportCcBandwidthState::Hold;
            estimate.reason = TransportCcEstimateReason::Stable;
        } else {
            const auto useStartupProbe = !lastEstimate_ || estimatorState_ == TransportCcEstimatorState::Startup
                                         || estimatorState_ == TransportCcEstimatorState::Probing;
            auto shouldStartProbeCluster = false;
            if (useStartupProbe) {
                estimate.targetBitrateBps = static_cast<uint64_t>(
                    std::lround(static_cast<double>(workingBitrateBps) * config_.startupIncreaseRatio));
                shouldStartProbeCluster = true;
            } else if (recoveringFromOveruse_ && cleanRecoveryReports_ >= config_.recoveryProbeCleanReports) {
                estimate.targetBitrateBps = static_cast<uint64_t>(
                    std::lround(static_cast<double>(workingBitrateBps) * config_.recoveryIncreaseRatio));
                shouldStartProbeCluster = true;
            } else {
                const auto ratioTargetBitrateBps = static_cast<uint64_t>(
                    std::lround(static_cast<double>(workingBitrateBps) * config_.increaseRatio));
                const auto additiveTargetBitrateBps = workingBitrateBps + config_.steadyIncreaseStepBps;
                estimate.targetBitrateBps = std::min(ratioTargetBitrateBps, additiveTargetBitrateBps);
            }
            estimate.state = TransportCcBandwidthState::Increase;
            estimate.reason = TransportCcEstimateReason::Stable;
            if (shouldStartProbeCluster)
                startProbeCluster(estimate);
        }
    } else {
        estimate.targetBitrateBps = workingBitrateBps;
        estimate.state = TransportCcBandwidthState::Hold;
        estimate.reason = isAppLimited ? TransportCcEstimateReason::AppLimited : TransportCcEstimateReason::Stable;
    }

    estimate.targetBitrateBps = clampBitrate(estimate.targetBitrateBps);
    estimate.estimatorState = selectEstimatorState(estimate,
                                                   workingBitrateBps,
                                                   hasLossOveruse,
                                                   hasPersistentDelayOveruse,
                                                   isAppLimited,
                                                   isUnderusing);
    targetBitrateBps_ = estimate.targetBitrateBps;
    lastEstimate_ = estimate;
    return estimate;
}

void
TransportCcController::setBitrateBounds(uint64_t minBitrateBps, uint64_t maxBitrateBps)
{
    if (maxBitrateBps && minBitrateBps && maxBitrateBps < minBitrateBps)
        maxBitrateBps = minBitrateBps;
    config_.minBitrateBps = minBitrateBps;
    config_.maxBitrateBps = maxBitrateBps;

    if (targetBitrateBps_)
        targetBitrateBps_ = clampBitrate(targetBitrateBps_);
    if (lastEstimate_)
        lastEstimate_->targetBitrateBps = clampBitrate(lastEstimate_->targetBitrateBps);
}

void
TransportCcController::reset()
{
    targetBitrateBps_ = 0;
    estimatorState_ = TransportCcEstimatorState::Startup;
    successfulStartupProbes_ = 0;
    recoveringFromOveruse_ = false;
    recoveryTargetBitrateBps_ = 0;
    cleanRecoveryReports_ = 0;
    increaseCooldownReports_ = 0;
    delayTrendWindow_.clear();
    accumulatedDelayUs_ = 0;
    minAccumulatedDelayUs_ = 0;
    hasMinAccumulatedDelay_ = false;
    nextDelayTrendSampleIndex_ = 0;
    lastDelayTrendReceiveTimeUs_.reset();
    acknowledgedBitrateWindow_.clear();
    lastAcknowledgedBitrateReceiveTimeUs_.reset();
    lastFeedbackPacketCount_.reset();
    lastFeedbackMaxSequenceNumber_.reset();
    lastFeedbackMediaSsrc_.reset();
    smoothedDelayTrendUs_ = 0;
    smoothedLossRatio_ = 0;
    hasSmoothedDelayTrend_ = false;
    hasSmoothedLossRatio_ = false;
    delayOveruseCount_ = 0;
    nextProbeClusterId_ = 1;
    failedProbeClusters_ = 0;
    pendingProbeCluster_.reset();
    lastEstimate_.reset();
}

uint64_t
TransportCcController::clampBitrate(uint64_t bitrateBps) const
{
    if (config_.minBitrateBps)
        bitrateBps = std::max(bitrateBps, config_.minBitrateBps);
    if (config_.maxBitrateBps)
        bitrateBps = std::min(bitrateBps, config_.maxBitrateBps);
    return bitrateBps;
}

double
TransportCcController::smooth(double previousValue, double newValue, double smoothingFactor) const
{
    const auto previousWeight = std::clamp(smoothingFactor, 0.0, 1.0);
    return previousValue * previousWeight + newValue * (1.0 - previousWeight);
}

void
TransportCcController::addDelayTrendSample(int64_t receiveTimeUs, int64_t delayDeltaUs)
{
    if (lastDelayTrendReceiveTimeUs_
        && (receiveTimeUs < *lastDelayTrendReceiveTimeUs_
            || receiveTimeUs - *lastDelayTrendReceiveTimeUs_ > config_.estimatorResetIntervalUs)) {
        delayTrendWindow_.clear();
        accumulatedDelayUs_ = 0;
        minAccumulatedDelayUs_ = 0;
        hasMinAccumulatedDelay_ = false;
        nextDelayTrendSampleIndex_ = 0;
    }

    accumulatedDelayUs_ += static_cast<double>(delayDeltaUs);
    if (!hasMinAccumulatedDelay_) {
        minAccumulatedDelayUs_ = accumulatedDelayUs_;
        hasMinAccumulatedDelay_ = true;
    } else {
        minAccumulatedDelayUs_ = std::min(minAccumulatedDelayUs_, accumulatedDelayUs_);
    }
    delayTrendWindow_.push_back({nextDelayTrendSampleIndex_++, accumulatedDelayUs_});
    lastDelayTrendReceiveTimeUs_ = receiveTimeUs;

    const auto maxWindowSize = std::max<size_t>(1, config_.delayTrendWindowSize);
    while (delayTrendWindow_.size() > maxWindowSize)
        delayTrendWindow_.pop_front();
}

std::optional<double>
TransportCcController::delayTrendlineUs() const
{
    if (delayTrendWindow_.size() < std::max<size_t>(2, config_.delayTrendMinSamples))
        return std::nullopt;

    double sumX = 0;
    double sumY = 0;
    for (const auto& sample : delayTrendWindow_) {
        sumX += static_cast<double>(sample.index);
        sumY += sample.accumulatedDelayUs;
    }

    const auto meanX = sumX / static_cast<double>(delayTrendWindow_.size());
    const auto meanY = sumY / static_cast<double>(delayTrendWindow_.size());
    double covariance = 0;
    double variance = 0;
    for (const auto& sample : delayTrendWindow_) {
        const auto x = static_cast<double>(sample.index) - meanX;
        const auto y = sample.accumulatedDelayUs - meanY;
        covariance += x * y;
        variance += x * x;
    }

    if (variance <= 0)
        return std::nullopt;
    return covariance / variance;
}

void
TransportCcController::addAcknowledgedBitrateSample(int64_t receiveTimeUs, size_t payloadSize)
{
    if (lastAcknowledgedBitrateReceiveTimeUs_
        && (receiveTimeUs < *lastAcknowledgedBitrateReceiveTimeUs_
            || receiveTimeUs - *lastAcknowledgedBitrateReceiveTimeUs_ > config_.estimatorResetIntervalUs)) {
        acknowledgedBitrateWindow_.clear();
    }

    acknowledgedBitrateWindow_.push_back({receiveTimeUs, payloadSize});
    lastAcknowledgedBitrateReceiveTimeUs_ = receiveTimeUs;

    const auto windowUs = std::max<int64_t>(1, config_.acknowledgedBitrateWindowUs);
    while (acknowledgedBitrateWindow_.size() > 1
           && receiveTimeUs - acknowledgedBitrateWindow_.front().receiveTimeUs > windowUs) {
        acknowledgedBitrateWindow_.pop_front();
    }
}

std::optional<uint64_t>
TransportCcController::acknowledgedBitrateBps() const
{
    if (acknowledgedBitrateWindow_.size() < std::max<size_t>(2, config_.acknowledgedBitrateMinPackets))
        return std::nullopt;

    const auto receiveSpanUs = acknowledgedBitrateWindow_.back().receiveTimeUs
                               - acknowledgedBitrateWindow_.front().receiveTimeUs;
    if (receiveSpanUs <= 0)
        return std::nullopt;

    uint64_t receivedBytes = 0;
    for (const auto& sample : acknowledgedBitrateWindow_)
        receivedBytes += sample.payloadSize;
    return static_cast<uint64_t>((receivedBytes * 8 * 1'000'000ULL) / static_cast<uint64_t>(receiveSpanUs));
}

TransportCcEstimatorState
TransportCcController::selectEstimatorState(const TransportCcControllerEstimate& estimate,
                                            uint64_t workingBitrateBps,
                                            bool hasLossOveruse,
                                            bool hasPersistentDelayOveruse,
                                            bool isAppLimited,
                                            bool isUnderusing)
{
    if (hasLossOveruse || hasPersistentDelayOveruse) {
        recoveringFromOveruse_ = true;
        recoveryTargetBitrateBps_ = std::max(recoveryTargetBitrateBps_, workingBitrateBps);
        successfulStartupProbes_ = 0;
        cleanRecoveryReports_ = 0;
        estimatorState_ = TransportCcEstimatorState::Overusing;
        return estimatorState_;
    }

    if (isAppLimited) {
        estimatorState_ = TransportCcEstimatorState::AppLimited;
        return estimatorState_;
    }

    if (recoveringFromOveruse_) {
        const auto recoveryExitBitrateBps = static_cast<uint64_t>(
            std::lround(static_cast<double>(recoveryTargetBitrateBps_) * config_.recoveryExitRatio));
        if (estimate.targetBitrateBps >= recoveryExitBitrateBps) {
            recoveringFromOveruse_ = false;
            recoveryTargetBitrateBps_ = 0;
        } else {
            estimatorState_ = TransportCcEstimatorState::Recovering;
            return estimatorState_;
        }
    }

    if (isUnderusing) {
        estimatorState_ = TransportCcEstimatorState::Underusing;
        return estimatorState_;
    }

    if (!lastEstimate_) {
        estimatorState_ = TransportCcEstimatorState::Startup;
        if (estimate.state == TransportCcBandwidthState::Increase)
            ++successfulStartupProbes_;
        return estimatorState_;
    }

    if (estimate.state == TransportCcBandwidthState::Increase) {
        if (successfulStartupProbes_ < config_.startupProbeCount)
            ++successfulStartupProbes_;
        estimatorState_ = successfulStartupProbes_ <= config_.startupProbeCount ? TransportCcEstimatorState::Probing
                                                                                : TransportCcEstimatorState::Normal;
        return estimatorState_;
    }

    estimatorState_ = TransportCcEstimatorState::Normal;
    return estimatorState_;
}

TransportCcLossType
TransportCcController::classifyLoss(const TransportCcControllerEstimate& estimate,
                                    double rawPacketLossRatio,
                                    bool hasDelayOveruse) const
{
    if (estimate.lostPackets == 0)
        return TransportCcLossType::None;

    const auto hasCongestionSignal = hasDelayOveruse
                                     || estimate.queueDelayUs >= config_.congestionQueueDelayThresholdUs;
    if (rawPacketLossRatio > config_.lossThresholdRatio && hasCongestionSignal)
        return TransportCcLossType::Congestion;

    if (estimate.maxConsecutiveLostPackets >= config_.burstLossPacketThreshold)
        return TransportCcLossType::Burst;

    return TransportCcLossType::Random;
}

void
TransportCcController::updateFeedbackState(const std::vector<uint8_t>& feedbackPacketCounts,
                                           TransportCcControllerEstimate& estimate)
{
    for (const auto feedbackPacketCount : feedbackPacketCounts) {
        if (!lastFeedbackPacketCount_) {
            lastFeedbackPacketCount_ = feedbackPacketCount;
            continue;
        }

        if (feedbackPacketCount < *lastFeedbackPacketCount_) {
            const auto wrappedDelta = static_cast<uint8_t>(feedbackPacketCount - *lastFeedbackPacketCount_);
            const auto backwardDelta = static_cast<uint8_t>(*lastFeedbackPacketCount_ - feedbackPacketCount);
            if (wrappedDelta > config_.feedbackCounterWrapTolerance
                && backwardDelta >= config_.feedbackCounterResetThreshold) {
                ++estimate.reorderedFeedbackReports;
                lastFeedbackPacketCount_ = feedbackPacketCount;
                continue;
            }
        }

        const auto delta = static_cast<uint8_t>(feedbackPacketCount - *lastFeedbackPacketCount_);
        if (delta == 0) {
            ++estimate.reorderedFeedbackReports;
            continue;
        }
        if (delta > 127) {
            ++estimate.reorderedFeedbackReports;
            continue;
        }
        if (delta > 1)
            estimate.missingFeedbackReports += delta - 1;
        lastFeedbackPacketCount_ = feedbackPacketCount;
    }

    if (estimate.missingFeedbackReports != 0)
        estimate.feedbackState = TransportCcFeedbackState::Missing;
    else if (estimate.reorderedFeedbackReports != 0)
        estimate.feedbackState = TransportCcFeedbackState::Reordered;
    else
        estimate.feedbackState = TransportCcFeedbackState::Normal;
}

void
TransportCcController::updateFeedbackSequenceState(const std::vector<uint16_t>& feedbackMaxSequenceNumbers,
                                                   TransportCcControllerEstimate& estimate)
{
    for (const auto sequenceNumber : feedbackMaxSequenceNumbers) {
        if (!lastFeedbackMaxSequenceNumber_) {
            lastFeedbackMaxSequenceNumber_ = sequenceNumber;
            continue;
        }
        if (!sequenceNumberIsNewer(sequenceNumber, *lastFeedbackMaxSequenceNumber_)) {
            ++estimate.reorderedFeedbackReports;
            continue;
        }
        lastFeedbackMaxSequenceNumber_ = sequenceNumber;
    }

    if (estimate.missingFeedbackReports != 0)
        estimate.feedbackState = TransportCcFeedbackState::Missing;
    else if (estimate.reorderedFeedbackReports != 0)
        estimate.feedbackState = TransportCcFeedbackState::Reordered;
    else
        estimate.feedbackState = TransportCcFeedbackState::Normal;
}

void
TransportCcController::resetOnFeedbackMediaSsrcChange(const std::list<TransportCcReport>& reports)
{
    for (const auto& report : reports) {
        if (report.feedback.mediaSsrc == 0)
            continue;
        if (!lastFeedbackMediaSsrc_) {
            lastFeedbackMediaSsrc_ = report.feedback.mediaSsrc;
            continue;
        }
        if (*lastFeedbackMediaSsrc_ == report.feedback.mediaSsrc)
            continue;

        const auto mediaSsrc = report.feedback.mediaSsrc;
        reset();
        lastFeedbackMediaSsrc_ = mediaSsrc;
    }
}

void
TransportCcController::validatePendingProbeCluster(TransportCcControllerEstimate& estimate,
                                                   size_t totalPackets,
                                                   int64_t sendDurationUs,
                                                   bool hasDelayOveruse)
{
    estimate.failedProbeClusters = failedProbeClusters_;
    if (!pendingProbeCluster_)
        return;

    estimate.probeObservedDurationUs = sendDurationUs;
    if (totalPackets < pendingProbeCluster_->minPackets || sendDurationUs < pendingProbeCluster_->durationUs)
        return;

    estimate.validatedProbeClusterId = pendingProbeCluster_->id;
    estimate.probeResultValid = true;
    const auto successfulBitrate = estimate.acknowledgedBitrateBps >= static_cast<uint64_t>(
                                       std::lround(static_cast<double>(pendingProbeCluster_->targetBitrateBps)
                                                   * config_.probeClusterSuccessRatio));
    estimate.probeSucceeded = successfulBitrate && estimate.packetLossRatio <= config_.lossThresholdRatio
                              && !hasDelayOveruse;
    if (!estimate.probeSucceeded)
        ++failedProbeClusters_;
    estimate.failedProbeClusters = failedProbeClusters_;
    pendingProbeCluster_.reset();
}

void
TransportCcController::startProbeCluster(TransportCcControllerEstimate& estimate)
{
    if (pendingProbeCluster_)
        return;

    pendingProbeCluster_ = {nextProbeClusterId_++,
                            estimate.targetBitrateBps,
                            config_.probeClusterDurationUs,
                            config_.probeClusterMinPackets};
    estimate.probeClusterId = pendingProbeCluster_->id;
    estimate.probeTargetBitrateBps = pendingProbeCluster_->targetBitrateBps;
    estimate.probeDurationUs = pendingProbeCluster_->durationUs;
    estimate.probeMinPackets = pendingProbeCluster_->minPackets;
    estimate.failedProbeClusters = failedProbeClusters_;
}

double
TransportCcController::estimateConfidence(size_t totalPackets,
                                          uint64_t acknowledgedBitrateBps,
                                          const TransportCcControllerEstimate& estimate) const
{
    if (totalPackets == 0)
        return 0.0;

    auto confidence = std::min(1.0,
                               static_cast<double>(totalPackets)
                                   / static_cast<double>(std::max<size_t>(1, config_.highConfidencePacketCount)));
    if (acknowledgedBitrateBps == 0)
        confidence *= 0.5;
    if (estimate.missingFeedbackReports != 0)
        confidence *= std::clamp(1.0 - config_.missingFeedbackConfidencePenalty, 0.0, 1.0);
    if (estimate.reorderedFeedbackReports != 0)
        confidence *= std::clamp(1.0 - config_.reorderedFeedbackConfidencePenalty, 0.0, 1.0);
    if (estimate.feedbackAgeUs > config_.staleFeedbackAgeUs)
        confidence *= std::clamp(1.0 - config_.staleFeedbackConfidencePenalty, 0.0, 1.0);
    if (estimate.missingSendHistoryPackets != 0)
        confidence *= std::clamp(1.0 - config_.missingSendHistoryConfidencePenalty, 0.0, 1.0);
    return std::clamp(confidence, 0.0, 1.0);
}

} // namespace jami