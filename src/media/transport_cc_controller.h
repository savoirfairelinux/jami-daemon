#ifndef JAMI_MEDIA_TRANSPORT_CC_CONTROLLER_H
#define JAMI_MEDIA_TRANSPORT_CC_CONTROLLER_H

#include "transport_cc.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <list>
#include <optional>

namespace jami {

enum class TransportCcBandwidthState : uint8_t { Hold, Increase, Decrease };

enum class TransportCcEstimatorState : uint8_t {
    Startup,
    Probing,
    Normal,
    Overusing,
    Underusing,
    Recovering,
    AppLimited
};

enum class TransportCcEstimateReason : uint8_t { Stable, DelayIncrease, PacketLoss, AppLimited };

enum class TransportCcLossType : uint8_t { None, Random, Burst, Congestion };

enum class TransportCcFeedbackState : uint8_t { Normal, Missing, Reordered };

struct TransportCcControllerConfig
{
    uint64_t minBitrateBps {30'000};
    uint64_t maxBitrateBps {5'000'000};
    double increaseRatio {1.08};
    double startupIncreaseRatio {1.20};
    uint64_t steadyIncreaseStepBps {50'000};
    double delayDecreaseRatio {0.85};
    double lossDecreaseFloor {0.50};
    double lossThresholdRatio {0.02};
    double increaseUtilizationThreshold {0.85};
    double delayTrendSmoothingFactor {0.50};
    double lossSmoothingFactor {0.50};
    int64_t delayTrendThresholdUs {15'000};
    unsigned delayOveruseCountThreshold {2};
    int64_t delayTrendGroupLengthUs {5'000};
    int64_t delayTrendOutlierThresholdUs {100'000};
    size_t delayTrendWindowSize {20};
    size_t delayTrendMinSamples {5};
    int64_t acknowledgedBitrateWindowUs {1'000'000};
    size_t acknowledgedBitrateMinPackets {2};
    int64_t estimatorResetIntervalUs {2'000'000};
    int64_t underuseTrendThresholdUs {-10'000};
    unsigned startupProbeCount {3};
    double recoveryExitRatio {0.95};
    unsigned recoveryProbeCleanReports {2};
    double recoveryIncreaseRatio {1.12};
    int64_t probeClusterDurationUs {100'000};
    size_t probeClusterMinPackets {5};
    double probeClusterSuccessRatio {0.90};
    unsigned increaseCooldownAfterDecreaseReports {1};
    size_t burstLossPacketThreshold {3};
    int64_t congestionQueueDelayThresholdUs {50'000};
    size_t highConfidencePacketCount {20};
    double minConfidenceForIncrease {0.50};
    double randomLossDecreaseThresholdRatio {0.10};
    double burstLossDecreaseThresholdRatio {0.05};
    double missingFeedbackConfidencePenalty {0.25};
    double reorderedFeedbackConfidencePenalty {0.50};
    int64_t staleFeedbackAgeUs {500'000};
    double staleFeedbackConfidencePenalty {0.50};
    double senderLimitedConfidencePenalty {0.50};
    double missingSendHistoryConfidencePenalty {0.50};
    uint8_t feedbackCounterWrapTolerance {16};
    uint8_t feedbackCounterResetThreshold {64};
};

struct TransportCcControllerEstimate
{
    uint64_t targetBitrateBps {};
    uint64_t acknowledgedBitrateBps {};
    uint64_t sentBitrateBps {};
    double packetLossRatio {};
    size_t receivedPackets {};
    size_t lostPackets {};
    int64_t delayTrendUs {};
    int64_t queueDelayUs {};
    size_t maxConsecutiveLostPackets {};
    size_t missingFeedbackReports {};
    size_t reorderedFeedbackReports {};
    size_t missingSendHistoryPackets {};
    int64_t feedbackAgeUs {};
    uint32_t probeClusterId {};
    uint64_t probeTargetBitrateBps {};
    int64_t probeDurationUs {};
    int64_t probeObservedDurationUs {};
    size_t probeMinPackets {};
    uint32_t validatedProbeClusterId {};
    bool probeResultValid {false};
    bool probeSucceeded {false};
    uint32_t failedProbeClusters {};
    double confidence {};
    TransportCcBandwidthState state {TransportCcBandwidthState::Hold};
    TransportCcEstimatorState estimatorState {TransportCcEstimatorState::Startup};
    TransportCcEstimateReason reason {TransportCcEstimateReason::Stable};
    TransportCcLossType lossType {TransportCcLossType::None};
    TransportCcFeedbackState feedbackState {TransportCcFeedbackState::Normal};
};

class TransportCcController
{
public:
    explicit TransportCcController(TransportCcControllerConfig config = {});

    std::optional<TransportCcControllerEstimate> update(uint64_t currentBitrateBps,
                                                        const std::list<TransportCcReport>& reports);
    void setBitrateBounds(uint64_t minBitrateBps, uint64_t maxBitrateBps);
    const std::optional<TransportCcControllerEstimate>& lastEstimate() const { return lastEstimate_; }
    uint64_t currentTargetBitrateBps() const { return targetBitrateBps_; }
    void reset();

private:
    struct DelayTrendSample
    {
        size_t index {};
        double accumulatedDelayUs {};
    };

    struct AcknowledgedBitrateSample
    {
        int64_t receiveTimeUs {};
        size_t payloadSize {};
    };

    struct ProbeCluster
    {
        uint32_t id {};
        uint64_t targetBitrateBps {};
        int64_t durationUs {};
        size_t minPackets {};
    };

    uint64_t clampBitrate(uint64_t bitrateBps) const;
    double smooth(double previousValue, double newValue, double smoothingFactor) const;
    void addDelayTrendSample(int64_t receiveTimeUs, int64_t delayDeltaUs);
    std::optional<double> delayTrendlineUs() const;
    void addAcknowledgedBitrateSample(int64_t receiveTimeUs, size_t payloadSize);
    std::optional<uint64_t> acknowledgedBitrateBps() const;
    TransportCcEstimatorState selectEstimatorState(const TransportCcControllerEstimate& estimate,
                                                   uint64_t workingBitrateBps,
                                                   bool hasLossOveruse,
                                                   bool hasPersistentDelayOveruse,
                                                   bool isAppLimited,
                                                   bool isUnderusing);
    TransportCcLossType classifyLoss(const TransportCcControllerEstimate& estimate,
                                     double rawPacketLossRatio,
                                     bool hasDelayOveruse) const;
    void updateFeedbackState(const std::vector<uint8_t>& feedbackPacketCounts, TransportCcControllerEstimate& estimate);
    void updateFeedbackSequenceState(const std::vector<uint16_t>& feedbackMaxSequenceNumbers,
                                     TransportCcControllerEstimate& estimate);
    void resetOnFeedbackMediaSsrcChange(const std::list<TransportCcReport>& reports);
    void validatePendingProbeCluster(TransportCcControllerEstimate& estimate,
                                     size_t totalPackets,
                                     int64_t sendDurationUs,
                                     bool hasDelayOveruse);
    void startProbeCluster(TransportCcControllerEstimate& estimate);
    double estimateConfidence(size_t totalPackets,
                              uint64_t acknowledgedBitrateBps,
                              const TransportCcControllerEstimate& estimate) const;

    TransportCcControllerConfig config_ {};
    uint64_t targetBitrateBps_ {};
    TransportCcEstimatorState estimatorState_ {TransportCcEstimatorState::Startup};
    unsigned successfulStartupProbes_ {0};
    bool recoveringFromOveruse_ {false};
    uint64_t recoveryTargetBitrateBps_ {};
    unsigned cleanRecoveryReports_ {0};
    unsigned increaseCooldownReports_ {0};
    std::deque<DelayTrendSample> delayTrendWindow_ {};
    double accumulatedDelayUs_ {};
    double minAccumulatedDelayUs_ {};
    bool hasMinAccumulatedDelay_ {false};
    size_t nextDelayTrendSampleIndex_ {};
    std::optional<int64_t> lastDelayTrendReceiveTimeUs_ {};
    std::deque<AcknowledgedBitrateSample> acknowledgedBitrateWindow_ {};
    std::optional<int64_t> lastAcknowledgedBitrateReceiveTimeUs_ {};
    std::optional<uint8_t> lastFeedbackPacketCount_ {};
    std::optional<uint16_t> lastFeedbackMaxSequenceNumber_ {};
    std::optional<uint32_t> lastFeedbackMediaSsrc_ {};
    double smoothedDelayTrendUs_ {};
    double smoothedLossRatio_ {};
    bool hasSmoothedDelayTrend_ {false};
    bool hasSmoothedLossRatio_ {false};
    unsigned delayOveruseCount_ {0};
    uint32_t nextProbeClusterId_ {1};
    uint32_t failedProbeClusters_ {0};
    std::optional<ProbeCluster> pendingProbeCluster_ {};
    std::optional<TransportCcControllerEstimate> lastEstimate_ {};
};

} // namespace jami

#endif // JAMI_MEDIA_TRANSPORT_CC_CONTROLLER_H