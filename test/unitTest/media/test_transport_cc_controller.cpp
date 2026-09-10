#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <list>
#include <vector>

#include "media/transport_cc_controller.h"
#include "test_runner.h"

namespace jami {
namespace test {
namespace {

uint8_t
nextFeedbackPacketCount()
{
    static uint8_t feedbackPacketCount = 0;
    return feedbackPacketCount++;
}

TransportCcReport
makeReport(uint16_t firstSequenceNumber,
           size_t packetCount,
           size_t lostEvery,
           int64_t sendIntervalUs,
           int64_t receiveIntervalUs,
           int64_t receiveStartUs = 0)
{
    TransportCcReport report;
    report.feedback.feedbackPacketCount = nextFeedbackPacketCount();
    for (size_t packetIndex = 0; packetIndex < packetCount; ++packetIndex) {
        const auto sequenceNumber = static_cast<uint16_t>(firstSequenceNumber + packetIndex);
        const auto lost = lostEvery != 0 && (packetIndex + 1) % lostEvery == 0;
        report.packets.push_back({sequenceNumber,
                                  lost ? TransportCcPacketStatus::NotReceived : TransportCcPacketStatus::SmallDelta,
                                  1200,
                                  static_cast<int64_t>(packetIndex) * sendIntervalUs,
                                  lost ? 0 : receiveStartUs + static_cast<int64_t>(packetIndex) * receiveIntervalUs});
    }
    return report;
}

TransportCcReport
makeUtilizedCleanReport(uint16_t firstSequenceNumber, uint64_t bitrateBps, double utilizationRatio)
{
    constexpr size_t packetCount = 20;
    constexpr uint64_t payloadSize = 1200;
    const auto acknowledgedBitrateBps = static_cast<uint64_t>(static_cast<double>(bitrateBps) * utilizationRatio);
    const auto receiveSpanUs = static_cast<int64_t>((packetCount * payloadSize * 8 * 1'000'000ULL)
                                                    / acknowledgedBitrateBps);
    const auto packetIntervalUs = receiveSpanUs / static_cast<int64_t>(packetCount - 1);
    return makeReport(firstSequenceNumber, packetCount, 0, packetIntervalUs, packetIntervalUs);
}

int64_t
packetIntervalUsForBitrate(uint64_t bitrateBps)
{
    constexpr uint64_t payloadSize = 1200;
    return std::max<int64_t>(1,
                             static_cast<int64_t>((payloadSize * 8 * 1'000'000ULL) / std::max<uint64_t>(1, bitrateBps)));
}

TransportCcReport
makeCapacityReport(uint16_t firstSequenceNumber, uint64_t sendBitrateBps, uint64_t capacityBps, int64_t receiveStartUs)
{
    const auto deliveredBitrateBps = std::min(sendBitrateBps, capacityBps);
    return makeReport(firstSequenceNumber,
                      20,
                      0,
                      packetIntervalUsForBitrate(sendBitrateBps),
                      packetIntervalUsForBitrate(deliveredBitrateBps),
                      receiveStartUs);
}

TransportCcReport
makeBurstLossReport(uint16_t firstSequenceNumber, size_t packetCount, size_t burstStart, size_t burstLength)
{
    TransportCcReport report;
    report.feedback.feedbackPacketCount = nextFeedbackPacketCount();
    for (size_t packetIndex = 0; packetIndex < packetCount; ++packetIndex) {
        const auto sequenceNumber = static_cast<uint16_t>(firstSequenceNumber + packetIndex);
        const auto lost = packetIndex >= burstStart && packetIndex < burstStart + burstLength;
        report.packets.push_back({sequenceNumber,
                                  lost ? TransportCcPacketStatus::NotReceived : TransportCcPacketStatus::SmallDelta,
                                  1200,
                                  static_cast<int64_t>(packetIndex) * 10'000,
                                  lost ? 0 : static_cast<int64_t>(packetIndex) * 10'000});
    }
    return report;
}

TransportCcReport
makeTimedReport(uint16_t firstSequenceNumber,
                const std::vector<int64_t>& sendTimesUs,
                const std::vector<int64_t>& receiveTimesUs)
{
    TransportCcReport report;
    report.feedback.feedbackPacketCount = nextFeedbackPacketCount();
    const auto packetCount = std::min(sendTimesUs.size(), receiveTimesUs.size());
    for (size_t packetIndex = 0; packetIndex < packetCount; ++packetIndex) {
        report.packets.push_back({static_cast<uint16_t>(firstSequenceNumber + packetIndex),
                                  TransportCcPacketStatus::SmallDelta,
                                  1200,
                                  sendTimesUs[packetIndex],
                                  receiveTimesUs[packetIndex]});
    }
    return report;
}

int64_t
steadyNowUs()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

} // namespace

class TransportCcControllerTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "transport_cc_controller"; }

private:
    void stableFeedbackIncreasesTowardAcknowledgedRate();
    void increasingDelayReducesTargetBitrate();
    void packetLossReducesTargetBitrate();
    void emptyFeedbackDoesNotChangeEstimate();
    void targetIsClampedToConfiguredBounds();
    void cleanFeedbackRecoversAfterOveruse();
    void appLimitedFeedbackDoesNotIncreaseTarget();
    void senderLimitedFeedbackReportsAppLimited();
    void multipleReportsAreAggregated();
    void isolatedDelaySpikeDoesNotReduceTarget();
    void cleanFeedbackDoesNotImmediatelyRecoverAfterLoss();
    void persistentDelayOveruseKeepsTargetBelowPreviousRate();
    void targetRecoversAfterCapacityDropClears();
    void nearCapacityCleanFeedbackProbesUpward();
    void cleanNearCapacityFeedbackRampsUpOverTime();
    void delayTrendlineWindowExpiresOldDelaySamples();
    void acknowledgedBitrateUsesRollingWindowForSparseFeedback();
    void cleanStartupFeedbackReportsStartupThenProbing();
    void startupProbingUsesConfiguredProbeRatio();
    void startupProbeCreatesProbeCluster();
    void probeClusterWaitsForMinimumDuration();
    void successfulProbeClusterIsReportedOnNextFeedback();
    void failedProbeClusterIsReportedOnNextFeedback();
    void normalIncreaseUsesConfiguredAdditiveStep();
    void recoveryProbeUsesConfiguredRecoveryRatio();
    void overuseThenCleanFeedbackReportsRecovery();
    void negativeDelayTrendReportsUnderusing();
    void queueDelayTracksMinimumDelayBaseline();
    void isolatedLossIsClassifiedAsRandom();
    void consecutiveLossIsClassifiedAsBurst();
    void delayedLossIsClassifiedAsCongestion();
    void missingFeedbackReportsAreTracked();
    void reorderedFeedbackReportsAreTracked();
    void feedbackCounterWrapIsHandled();
    void feedbackCounterResetDoesNotPoisonNextReport();
    void feedbackMediaSsrcChangeResetsEstimatorState();
    void lateFeedbackSequenceIsClassifiedAsReordered();
    void feedbackSequenceWrapIsHandled();
    void confidenceIncreasesWithPacketCount();
    void missingFeedbackLowersConfidence();
    void staleFeedbackLowersConfidence();
    void missingSendHistoryLowersConfidence();
    void delayTrendOutlierIsIgnored();
    void groupedDelayTrendDetectsClusteredQueueGrowth();
    void lowConfidenceFeedbackDoesNotIncreaseTarget();
    void smallRandomLossDoesNotReduceTarget();
    void queueDelayOveruseReducesTarget();
    void longFeedbackGapResetsDelayAndRateWindows();
    void simulatedCapacityDropAndRecoveryAdaptsTarget();
    void simulatedQueueGrowthTriggersDecrease();
    void simulatedCompetingTrafficKeepsTargetConservative();

    CPPUNIT_TEST_SUITE(TransportCcControllerTest);
    CPPUNIT_TEST(stableFeedbackIncreasesTowardAcknowledgedRate);
    CPPUNIT_TEST(increasingDelayReducesTargetBitrate);
    CPPUNIT_TEST(packetLossReducesTargetBitrate);
    CPPUNIT_TEST(emptyFeedbackDoesNotChangeEstimate);
    CPPUNIT_TEST(targetIsClampedToConfiguredBounds);
    CPPUNIT_TEST(cleanFeedbackRecoversAfterOveruse);
    CPPUNIT_TEST(appLimitedFeedbackDoesNotIncreaseTarget);
    CPPUNIT_TEST(senderLimitedFeedbackReportsAppLimited);
    CPPUNIT_TEST(multipleReportsAreAggregated);
    CPPUNIT_TEST(isolatedDelaySpikeDoesNotReduceTarget);
    CPPUNIT_TEST(cleanFeedbackDoesNotImmediatelyRecoverAfterLoss);
    CPPUNIT_TEST(persistentDelayOveruseKeepsTargetBelowPreviousRate);
    CPPUNIT_TEST(targetRecoversAfterCapacityDropClears);
    CPPUNIT_TEST(nearCapacityCleanFeedbackProbesUpward);
    CPPUNIT_TEST(cleanNearCapacityFeedbackRampsUpOverTime);
    CPPUNIT_TEST(delayTrendlineWindowExpiresOldDelaySamples);
    CPPUNIT_TEST(acknowledgedBitrateUsesRollingWindowForSparseFeedback);
    CPPUNIT_TEST(cleanStartupFeedbackReportsStartupThenProbing);
    CPPUNIT_TEST(startupProbingUsesConfiguredProbeRatio);
    CPPUNIT_TEST(startupProbeCreatesProbeCluster);
    CPPUNIT_TEST(probeClusterWaitsForMinimumDuration);
    CPPUNIT_TEST(successfulProbeClusterIsReportedOnNextFeedback);
    CPPUNIT_TEST(failedProbeClusterIsReportedOnNextFeedback);
    CPPUNIT_TEST(normalIncreaseUsesConfiguredAdditiveStep);
    CPPUNIT_TEST(recoveryProbeUsesConfiguredRecoveryRatio);
    CPPUNIT_TEST(overuseThenCleanFeedbackReportsRecovery);
    CPPUNIT_TEST(negativeDelayTrendReportsUnderusing);
    CPPUNIT_TEST(queueDelayTracksMinimumDelayBaseline);
    CPPUNIT_TEST(isolatedLossIsClassifiedAsRandom);
    CPPUNIT_TEST(consecutiveLossIsClassifiedAsBurst);
    CPPUNIT_TEST(delayedLossIsClassifiedAsCongestion);
    CPPUNIT_TEST(missingFeedbackReportsAreTracked);
    CPPUNIT_TEST(reorderedFeedbackReportsAreTracked);
    CPPUNIT_TEST(feedbackCounterWrapIsHandled);
    CPPUNIT_TEST(feedbackCounterResetDoesNotPoisonNextReport);
    CPPUNIT_TEST(feedbackMediaSsrcChangeResetsEstimatorState);
    CPPUNIT_TEST(lateFeedbackSequenceIsClassifiedAsReordered);
    CPPUNIT_TEST(feedbackSequenceWrapIsHandled);
    CPPUNIT_TEST(confidenceIncreasesWithPacketCount);
    CPPUNIT_TEST(missingFeedbackLowersConfidence);
    CPPUNIT_TEST(staleFeedbackLowersConfidence);
    CPPUNIT_TEST(missingSendHistoryLowersConfidence);
    CPPUNIT_TEST(delayTrendOutlierIsIgnored);
    CPPUNIT_TEST(groupedDelayTrendDetectsClusteredQueueGrowth);
    CPPUNIT_TEST(lowConfidenceFeedbackDoesNotIncreaseTarget);
    CPPUNIT_TEST(smallRandomLossDoesNotReduceTarget);
    CPPUNIT_TEST(queueDelayOveruseReducesTarget);
    CPPUNIT_TEST(longFeedbackGapResetsDelayAndRateWindows);
    CPPUNIT_TEST(simulatedCapacityDropAndRecoveryAdaptsTarget);
    CPPUNIT_TEST(simulatedQueueGrowthTriggersDecrease);
    CPPUNIT_TEST(simulatedCompetingTrafficKeepsTargetConservative);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(TransportCcControllerTest, TransportCcControllerTest::name());

void
TransportCcControllerTest::stableFeedbackIncreasesTowardAcknowledgedRate()
{
    TransportCcController controller;
    const auto report = makeReport(1000, 10, 0, 10'000, 10'000);

    const auto estimate = controller.update(500'000, std::list<TransportCcReport> {report});

    CPPUNIT_ASSERT(estimate);
    CPPUNIT_ASSERT(estimate->targetBitrateBps > 500'000);
    CPPUNIT_ASSERT(estimate->targetBitrateBps <= estimate->acknowledgedBitrateBps);
    CPPUNIT_ASSERT(estimate->state == TransportCcBandwidthState::Increase);
    CPPUNIT_ASSERT(estimate->reason == TransportCcEstimateReason::Stable);
    CPPUNIT_ASSERT_EQUAL(size_t(10), estimate->receivedPackets);
    CPPUNIT_ASSERT_EQUAL(size_t(0), estimate->lostPackets);
}

void
TransportCcControllerTest::increasingDelayReducesTargetBitrate()
{
    TransportCcController controller;
    const auto report = makeReport(2000, 8, 0, 10'000, 35'000);

    controller.update(1'000'000, std::list<TransportCcReport> {report});
    const auto estimate = controller.update(1'000'000, std::list<TransportCcReport> {report});

    CPPUNIT_ASSERT(estimate);
    CPPUNIT_ASSERT(estimate->targetBitrateBps < 1'000'000);
    CPPUNIT_ASSERT(estimate->delayTrendUs > 15'000);
    CPPUNIT_ASSERT(estimate->state == TransportCcBandwidthState::Decrease);
    CPPUNIT_ASSERT(estimate->reason == TransportCcEstimateReason::DelayIncrease);
}

void
TransportCcControllerTest::packetLossReducesTargetBitrate()
{
    TransportCcController controller;
    const auto report = makeReport(3000, 10, 5, 10'000, 10'000);

    const auto estimate = controller.update(1'000'000, std::list<TransportCcReport> {report});

    CPPUNIT_ASSERT(estimate);
    CPPUNIT_ASSERT(estimate->targetBitrateBps < 1'000'000);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(0.2, estimate->packetLossRatio, 0.001);
    CPPUNIT_ASSERT(estimate->state == TransportCcBandwidthState::Decrease);
    CPPUNIT_ASSERT(estimate->reason == TransportCcEstimateReason::PacketLoss);
}

void
TransportCcControllerTest::emptyFeedbackDoesNotChangeEstimate()
{
    TransportCcController controller;

    const auto estimate = controller.update(1'000'000, {});

    CPPUNIT_ASSERT(!estimate);
    CPPUNIT_ASSERT(!controller.lastEstimate());
    CPPUNIT_ASSERT_EQUAL(uint64_t(0), controller.currentTargetBitrateBps());
}

void
TransportCcControllerTest::targetIsClampedToConfiguredBounds()
{
    TransportCcControllerConfig config;
    config.minBitrateBps = 300'000;
    config.maxBitrateBps = 800'000;
    TransportCcController controller(config);
    const auto lossReport = makeReport(4000, 10, 2, 10'000, 10'000);

    const auto lossEstimate = controller.update(320'000, std::list<TransportCcReport> {lossReport});

    CPPUNIT_ASSERT(lossEstimate);
    CPPUNIT_ASSERT_EQUAL(uint64_t(300'000), lossEstimate->targetBitrateBps);

    controller.reset();
    const auto cleanReport = makeReport(5000, 10, 0, 5'000, 5'000);
    const auto cleanEstimate = controller.update(790'000, std::list<TransportCcReport> {cleanReport});

    CPPUNIT_ASSERT(cleanEstimate);
    CPPUNIT_ASSERT_EQUAL(uint64_t(800'000), cleanEstimate->targetBitrateBps);
}

void
TransportCcControllerTest::cleanFeedbackRecoversAfterOveruse()
{
    TransportCcController controller;
    const auto delayReport = makeReport(6000, 8, 0, 10'000, 35'000);
    controller.update(1'000'000, std::list<TransportCcReport> {delayReport});
    const auto overuseEstimate = controller.update(1'000'000, std::list<TransportCcReport> {delayReport});

    CPPUNIT_ASSERT(overuseEstimate);
    CPPUNIT_ASSERT(overuseEstimate->targetBitrateBps < 1'000'000);

    const auto cooldownEstimate = controller.update(overuseEstimate->targetBitrateBps,
                                                    std::list<TransportCcReport> {
                                                        makeReport(7000, 10, 0, 5'000, 5'000)});
    CPPUNIT_ASSERT(cooldownEstimate);
    CPPUNIT_ASSERT_EQUAL(overuseEstimate->targetBitrateBps, cooldownEstimate->targetBitrateBps);
    CPPUNIT_ASSERT(cooldownEstimate->state == TransportCcBandwidthState::Hold);

    const auto recoveryEstimate = controller.update(cooldownEstimate->targetBitrateBps,
                                                    std::list<TransportCcReport> {
                                                        makeReport(7100, 10, 0, 5'000, 5'000, 100'000)});
    CPPUNIT_ASSERT(recoveryEstimate);
    CPPUNIT_ASSERT(recoveryEstimate->targetBitrateBps > overuseEstimate->targetBitrateBps);
    CPPUNIT_ASSERT(recoveryEstimate->state == TransportCcBandwidthState::Increase);
    CPPUNIT_ASSERT(recoveryEstimate->reason == TransportCcEstimateReason::Stable);
}

void
TransportCcControllerTest::appLimitedFeedbackDoesNotIncreaseTarget()
{
    TransportCcController controller;
    const auto report = makeReport(8000, 10, 0, 30'000, 30'000);

    const auto estimate = controller.update(1'000'000, std::list<TransportCcReport> {report});

    CPPUNIT_ASSERT(estimate);
    CPPUNIT_ASSERT_EQUAL(uint64_t(1'000'000), estimate->targetBitrateBps);
    CPPUNIT_ASSERT(estimate->acknowledgedBitrateBps < 1'000'000);
    CPPUNIT_ASSERT(estimate->state == TransportCcBandwidthState::Hold);
    CPPUNIT_ASSERT(estimate->estimatorState == TransportCcEstimatorState::AppLimited);
    CPPUNIT_ASSERT(estimate->reason == TransportCcEstimateReason::AppLimited);
}

void
TransportCcControllerTest::senderLimitedFeedbackReportsAppLimited()
{
    TransportCcController controller;
    const auto report = makeCapacityReport(7600, 300'000, 1'000'000, 0);

    const auto estimate = controller.update(1'000'000, std::list<TransportCcReport> {report});

    CPPUNIT_ASSERT(estimate);
    CPPUNIT_ASSERT(estimate->sentBitrateBps < 850'000);
    CPPUNIT_ASSERT(estimate->acknowledgedBitrateBps < 850'000);
    CPPUNIT_ASSERT_EQUAL(uint64_t(1'000'000), estimate->targetBitrateBps);
    CPPUNIT_ASSERT(estimate->confidence < 1.0);
    CPPUNIT_ASSERT(estimate->state == TransportCcBandwidthState::Hold);
    CPPUNIT_ASSERT(estimate->reason == TransportCcEstimateReason::AppLimited);
    CPPUNIT_ASSERT(estimate->estimatorState == TransportCcEstimatorState::AppLimited);
}

void
TransportCcControllerTest::multipleReportsAreAggregated()
{
    TransportCcController controller;
    auto firstReport = makeReport(9000, 5, 0, 10'000, 10'000);
    auto secondReport = makeReport(9005, 5, 5, 10'000, 10'000, 100'000);

    const auto estimate = controller.update(1'000'000, std::list<TransportCcReport> {firstReport, secondReport});

    CPPUNIT_ASSERT(estimate);
    CPPUNIT_ASSERT_EQUAL(size_t(9), estimate->receivedPackets);
    CPPUNIT_ASSERT_EQUAL(size_t(1), estimate->lostPackets);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(0.1, estimate->packetLossRatio, 0.001);
    CPPUNIT_ASSERT(estimate->state == TransportCcBandwidthState::Decrease);
}

void
TransportCcControllerTest::isolatedDelaySpikeDoesNotReduceTarget()
{
    TransportCcController controller;
    const auto spikeReport = makeReport(10'000, 8, 0, 10'000, 35'000);
    const auto cleanReport = makeReport(10'100, 8, 0, 10'000, 10'000);

    const auto spikeEstimate = controller.update(1'000'000, std::list<TransportCcReport> {spikeReport});
    const auto cleanEstimate = controller.update(1'000'000, std::list<TransportCcReport> {cleanReport});

    CPPUNIT_ASSERT(spikeEstimate);
    CPPUNIT_ASSERT(cleanEstimate);
    CPPUNIT_ASSERT(spikeEstimate->targetBitrateBps >= 1'000'000);
    CPPUNIT_ASSERT(cleanEstimate->targetBitrateBps >= 1'000'000);
    CPPUNIT_ASSERT(cleanEstimate->state != TransportCcBandwidthState::Decrease);
}

void
TransportCcControllerTest::cleanFeedbackDoesNotImmediatelyRecoverAfterLoss()
{
    TransportCcController controller;
    const auto lossReport = makeReport(11'000, 10, 5, 10'000, 10'000);
    const auto cleanReport = makeReport(11'100, 10, 0, 5'000, 5'000);

    const auto lossEstimate = controller.update(1'000'000, std::list<TransportCcReport> {lossReport});
    CPPUNIT_ASSERT(lossEstimate);
    const auto cleanEstimate = controller.update(lossEstimate->targetBitrateBps,
                                                 std::list<TransportCcReport> {cleanReport});

    CPPUNIT_ASSERT(cleanEstimate);
    CPPUNIT_ASSERT_EQUAL(lossEstimate->targetBitrateBps, cleanEstimate->targetBitrateBps);
    CPPUNIT_ASSERT(cleanEstimate->packetLossRatio > 0.02);
    CPPUNIT_ASSERT(cleanEstimate->state == TransportCcBandwidthState::Hold);
}

void
TransportCcControllerTest::persistentDelayOveruseKeepsTargetBelowPreviousRate()
{
    TransportCcController controller;
    const auto delayReport = makeReport(12'000, 10, 0, 10'000, 35'000);

    const auto firstEstimate = controller.update(1'000'000, std::list<TransportCcReport> {delayReport});
    const auto secondEstimate = controller.update(1'000'000, std::list<TransportCcReport> {delayReport});
    CPPUNIT_ASSERT(secondEstimate);
    const auto thirdEstimate = controller.update(secondEstimate->targetBitrateBps,
                                                 std::list<TransportCcReport> {delayReport});

    CPPUNIT_ASSERT(firstEstimate);
    CPPUNIT_ASSERT(thirdEstimate);
    CPPUNIT_ASSERT(firstEstimate->state != TransportCcBandwidthState::Decrease);
    CPPUNIT_ASSERT(secondEstimate->targetBitrateBps < 1'000'000);
    CPPUNIT_ASSERT(thirdEstimate->targetBitrateBps <= secondEstimate->targetBitrateBps);
    CPPUNIT_ASSERT(thirdEstimate->state == TransportCcBandwidthState::Decrease);
}

void
TransportCcControllerTest::targetRecoversAfterCapacityDropClears()
{
    TransportCcController controller;
    const auto delayReport = makeReport(13'000, 10, 0, 10'000, 35'000);
    controller.update(1'000'000, std::list<TransportCcReport> {delayReport});
    const auto reducedEstimate = controller.update(1'000'000, std::list<TransportCcReport> {delayReport});
    CPPUNIT_ASSERT(reducedEstimate);

    auto recoveredEstimate = controller.update(reducedEstimate->targetBitrateBps,
                                               std::list<TransportCcReport> {
                                                   makeReport(13'100, 10, 0, 5'000, 5'000, 0)});
    for (auto step = 0; step < 5; ++step) {
        CPPUNIT_ASSERT(recoveredEstimate);
        recoveredEstimate = controller.update(recoveredEstimate->targetBitrateBps,
                                              std::list<TransportCcReport> {
                                                  makeReport(static_cast<uint16_t>(13'200 + step * 100),
                                                             10,
                                                             0,
                                                             5'000,
                                                             5'000,
                                                             static_cast<int64_t>(step + 1) * 100'000)});
    }

    CPPUNIT_ASSERT(recoveredEstimate);
    CPPUNIT_ASSERT(recoveredEstimate->targetBitrateBps > reducedEstimate->targetBitrateBps);
    CPPUNIT_ASSERT(recoveredEstimate->targetBitrateBps <= recoveredEstimate->acknowledgedBitrateBps);
}

void
TransportCcControllerTest::nearCapacityCleanFeedbackProbesUpward()
{
    TransportCcController controller;
    const auto report = makeUtilizedCleanReport(14'000, 1'000'000, 0.95);

    const auto estimate = controller.update(1'000'000, std::list<TransportCcReport> {report});

    CPPUNIT_ASSERT(estimate);
    CPPUNIT_ASSERT(estimate->acknowledgedBitrateBps < 1'000'000);
    CPPUNIT_ASSERT(estimate->acknowledgedBitrateBps > 900'000);
    CPPUNIT_ASSERT(estimate->targetBitrateBps > 1'000'000);
    CPPUNIT_ASSERT(estimate->state == TransportCcBandwidthState::Increase);
}

void
TransportCcControllerTest::cleanNearCapacityFeedbackRampsUpOverTime()
{
    TransportCcController controller;
    auto targetBitrateBps = uint64_t(600'000);

    for (auto step = 0; step < 8; ++step) {
        const auto report = makeUtilizedCleanReport(static_cast<uint16_t>(15'000 + step * 100), targetBitrateBps, 0.95);
        const auto estimate = controller.update(targetBitrateBps, std::list<TransportCcReport> {report});

        CPPUNIT_ASSERT(estimate);
        CPPUNIT_ASSERT(estimate->targetBitrateBps > targetBitrateBps);
        CPPUNIT_ASSERT(estimate->state == TransportCcBandwidthState::Increase);
        targetBitrateBps = estimate->targetBitrateBps;
    }

    CPPUNIT_ASSERT(targetBitrateBps > 1'000'000);
}

void
TransportCcControllerTest::delayTrendlineWindowExpiresOldDelaySamples()
{
    TransportCcControllerConfig config;
    config.delayTrendWindowSize = 6;
    config.delayTrendMinSamples = 3;
    config.delayTrendSmoothingFactor = 0.0;
    TransportCcController controller(config);

    const auto firstDelayReport = makeReport(16'000, 8, 0, 10'000, 35'000, 0);
    const auto secondDelayReport = makeReport(16'100, 8, 0, 10'000, 35'000, 300'000);
    controller.update(1'000'000, std::list<TransportCcReport> {firstDelayReport});
    const auto reducedEstimate = controller.update(1'000'000, std::list<TransportCcReport> {secondDelayReport});
    CPPUNIT_ASSERT(reducedEstimate);
    CPPUNIT_ASSERT(reducedEstimate->state == TransportCcBandwidthState::Decrease);

    const auto cleanReport = makeReport(16'200, 12, 0, 8'000, 8'000, 700'000);
    const auto cleanEstimate = controller.update(reducedEstimate->targetBitrateBps,
                                                 std::list<TransportCcReport> {cleanReport});

    CPPUNIT_ASSERT(cleanEstimate);
    CPPUNIT_ASSERT(cleanEstimate->delayTrendUs <= config.delayTrendThresholdUs);
    CPPUNIT_ASSERT(cleanEstimate->state != TransportCcBandwidthState::Decrease);
}

void
TransportCcControllerTest::acknowledgedBitrateUsesRollingWindowForSparseFeedback()
{
    TransportCcController controller;
    const auto warmupReport = makeReport(17'000, 10, 0, 10'000, 10'000, 0);
    controller.update(500'000, std::list<TransportCcReport> {warmupReport});

    const auto sparseReport = makeReport(17'100, 1, 0, 10'000, 10'000, 100'000);
    const auto estimate = controller.update(500'000, std::list<TransportCcReport> {sparseReport});

    CPPUNIT_ASSERT(estimate);
    CPPUNIT_ASSERT(estimate->acknowledgedBitrateBps > 0);
    CPPUNIT_ASSERT(estimate->state == TransportCcBandwidthState::Hold);
}

void
TransportCcControllerTest::cleanStartupFeedbackReportsStartupThenProbing()
{
    TransportCcController controller;
    const auto firstEstimate = controller.update(600'000,
                                                 std::list<TransportCcReport> {
                                                     makeUtilizedCleanReport(18'000, 600'000, 0.95)});
    const auto secondEstimate = controller.update(firstEstimate->targetBitrateBps,
                                                  std::list<TransportCcReport> {
                                                      makeUtilizedCleanReport(18'100,
                                                                              firstEstimate->targetBitrateBps,
                                                                              0.95)});

    CPPUNIT_ASSERT(firstEstimate);
    CPPUNIT_ASSERT(secondEstimate);
    CPPUNIT_ASSERT(firstEstimate->estimatorState == TransportCcEstimatorState::Startup);
    CPPUNIT_ASSERT(secondEstimate->estimatorState == TransportCcEstimatorState::Probing);
}

void
TransportCcControllerTest::startupProbingUsesConfiguredProbeRatio()
{
    TransportCcControllerConfig config;
    config.increaseRatio = 1.05;
    config.startupIncreaseRatio = 1.20;
    TransportCcController controller(config);

    const auto estimate = controller.update(500'000,
                                            std::list<TransportCcReport> {makeReport(18'500, 10, 0, 10'000, 10'000)});

    CPPUNIT_ASSERT(estimate);
    CPPUNIT_ASSERT_EQUAL(uint64_t(600'000), estimate->targetBitrateBps);
    CPPUNIT_ASSERT(estimate->state == TransportCcBandwidthState::Increase);
    CPPUNIT_ASSERT(estimate->estimatorState == TransportCcEstimatorState::Startup);
}

void
TransportCcControllerTest::startupProbeCreatesProbeCluster()
{
    TransportCcControllerConfig config;
    config.startupIncreaseRatio = 1.20;
    config.probeClusterDurationUs = 80'000;
    config.probeClusterMinPackets = 8;
    TransportCcController controller(config);

    const auto estimate = controller.update(500'000,
                                            std::list<TransportCcReport> {makeReport(18'550, 10, 0, 10'000, 10'000)});

    CPPUNIT_ASSERT(estimate);
    CPPUNIT_ASSERT_EQUAL(uint32_t(1), estimate->probeClusterId);
    CPPUNIT_ASSERT_EQUAL(uint64_t(600'000), estimate->probeTargetBitrateBps);
    CPPUNIT_ASSERT_EQUAL(int64_t(80'000), estimate->probeDurationUs);
    CPPUNIT_ASSERT_EQUAL(size_t(8), estimate->probeMinPackets);
    CPPUNIT_ASSERT(!estimate->probeResultValid);
}

void
TransportCcControllerTest::probeClusterWaitsForMinimumDuration()
{
    TransportCcControllerConfig config;
    config.startupIncreaseRatio = 1.20;
    config.probeClusterDurationUs = 100'000;
    config.probeClusterMinPackets = 8;
    TransportCcController controller(config);

    const auto probeEstimate = controller.update(500'000,
                                                 std::list<TransportCcReport> {
                                                     makeReport(18'555, 10, 0, 10'000, 10'000, 0)});
    CPPUNIT_ASSERT(probeEstimate);
    CPPUNIT_ASSERT_EQUAL(uint32_t(1), probeEstimate->probeClusterId);

    const auto shortResultEstimate = controller.update(probeEstimate->targetBitrateBps,
                                                       std::list<TransportCcReport> {
                                                           makeReport(18'655, 10, 0, 1'000, 1'000, 100'000)});
    CPPUNIT_ASSERT(shortResultEstimate);
    CPPUNIT_ASSERT(!shortResultEstimate->probeResultValid);
    CPPUNIT_ASSERT_EQUAL(int64_t(9'000), shortResultEstimate->probeObservedDurationUs);
    CPPUNIT_ASSERT_EQUAL(uint32_t(0), shortResultEstimate->failedProbeClusters);
    CPPUNIT_ASSERT_EQUAL(uint32_t(0), shortResultEstimate->probeClusterId);

    const auto longResultEstimate = controller.update(shortResultEstimate->targetBitrateBps,
                                                      std::list<TransportCcReport> {
                                                          makeReport(18'755, 10, 0, 15'000, 15'000, 200'000)});
    CPPUNIT_ASSERT(longResultEstimate);
    CPPUNIT_ASSERT(longResultEstimate->probeResultValid);
    CPPUNIT_ASSERT_EQUAL(uint32_t(1), longResultEstimate->validatedProbeClusterId);
}

void
TransportCcControllerTest::successfulProbeClusterIsReportedOnNextFeedback()
{
    TransportCcControllerConfig config;
    config.startupIncreaseRatio = 1.20;
    config.probeClusterDurationUs = 100'000;
    config.probeClusterMinPackets = 8;
    config.probeClusterSuccessRatio = 0.90;
    TransportCcController controller(config);

    const auto probeEstimate = controller.update(500'000,
                                                 std::list<TransportCcReport> {
                                                     makeReport(18'558, 10, 0, 10'000, 10'000, 0)});
    CPPUNIT_ASSERT(probeEstimate);
    CPPUNIT_ASSERT_EQUAL(uint32_t(1), probeEstimate->probeClusterId);

    const auto resultEstimate = controller.update(probeEstimate->targetBitrateBps,
                                                  std::list<TransportCcReport> {
                                                      makeCapacityReport(18'658,
                                                                         probeEstimate->targetBitrateBps,
                                                                         probeEstimate->targetBitrateBps,
                                                                         120'000)});

    CPPUNIT_ASSERT(resultEstimate);
    CPPUNIT_ASSERT_EQUAL(uint32_t(1), resultEstimate->validatedProbeClusterId);
    CPPUNIT_ASSERT(resultEstimate->probeResultValid);
    CPPUNIT_ASSERT(resultEstimate->probeSucceeded);
    CPPUNIT_ASSERT_EQUAL(uint32_t(0), resultEstimate->failedProbeClusters);
}

void
TransportCcControllerTest::failedProbeClusterIsReportedOnNextFeedback()
{
    TransportCcControllerConfig config;
    config.startupIncreaseRatio = 1.20;
    config.probeClusterMinPackets = 8;
    config.probeClusterSuccessRatio = 0.90;
    config.delayTrendSmoothingFactor = 0.0;
    config.delayTrendThresholdUs = 5'000;
    config.delayOveruseCountThreshold = 1;
    TransportCcController controller(config);

    const auto probeEstimate = controller.update(500'000,
                                                 std::list<TransportCcReport> {
                                                     makeReport(18'560, 10, 0, 10'000, 10'000, 0)});
    CPPUNIT_ASSERT(probeEstimate);
    CPPUNIT_ASSERT_EQUAL(uint32_t(1), probeEstimate->probeClusterId);

    const auto resultEstimate
        = controller.update(probeEstimate->targetBitrateBps,
                            std::list<TransportCcReport> {
                                makeCapacityReport(18'660, probeEstimate->targetBitrateBps, 300'000, 200'000)});

    CPPUNIT_ASSERT(resultEstimate);
    CPPUNIT_ASSERT_EQUAL(uint32_t(1), resultEstimate->validatedProbeClusterId);
    CPPUNIT_ASSERT(resultEstimate->probeResultValid);
    CPPUNIT_ASSERT(!resultEstimate->probeSucceeded);
    CPPUNIT_ASSERT_EQUAL(uint32_t(1), resultEstimate->failedProbeClusters);
}

void
TransportCcControllerTest::normalIncreaseUsesConfiguredAdditiveStep()
{
    TransportCcControllerConfig config;
    config.startupProbeCount = 0;
    config.startupIncreaseRatio = 1.20;
    config.increaseRatio = 1.50;
    config.steadyIncreaseStepBps = 50'000;
    TransportCcController controller(config);

    const auto firstEstimate = controller.update(500'000,
                                                 std::list<TransportCcReport> {
                                                     makeReport(18'600, 10, 0, 10'000, 10'000, 0)});
    CPPUNIT_ASSERT(firstEstimate);
    const auto secondEstimate = controller.update(firstEstimate->targetBitrateBps,
                                                  std::list<TransportCcReport> {
                                                      makeReport(18'700, 10, 0, 10'000, 10'000, 100'000)});
    CPPUNIT_ASSERT(secondEstimate);
    CPPUNIT_ASSERT(secondEstimate->estimatorState == TransportCcEstimatorState::Normal);

    const auto thirdEstimate = controller.update(secondEstimate->targetBitrateBps,
                                                 std::list<TransportCcReport> {
                                                     makeReport(18'800, 10, 0, 10'000, 10'000, 200'000)});

    CPPUNIT_ASSERT(thirdEstimate);
    CPPUNIT_ASSERT_EQUAL(secondEstimate->targetBitrateBps + config.steadyIncreaseStepBps,
                         thirdEstimate->targetBitrateBps);
    CPPUNIT_ASSERT(thirdEstimate->state == TransportCcBandwidthState::Increase);
    CPPUNIT_ASSERT(thirdEstimate->estimatorState == TransportCcEstimatorState::Normal);
}

void
TransportCcControllerTest::recoveryProbeUsesConfiguredRecoveryRatio()
{
    TransportCcControllerConfig config;
    config.delayTrendSmoothingFactor = 0.0;
    config.delayTrendThresholdUs = 5'000;
    config.delayTrendMinSamples = 100;
    config.delayOveruseCountThreshold = 1;
    config.congestionQueueDelayThresholdUs = 1'000'000;
    config.recoveryProbeCleanReports = 2;
    config.recoveryIncreaseRatio = 1.12;
    config.steadyIncreaseStepBps = 25'000;
    TransportCcController controller(config);
    const auto delayReport = makeReport(18'900, 10, 0, 1'000, 12'000);

    const auto overuseEstimate = controller.update(1'000'000, std::list<TransportCcReport> {delayReport});
    CPPUNIT_ASSERT(overuseEstimate);
    CPPUNIT_ASSERT_EQUAL(uint64_t(850'000), overuseEstimate->targetBitrateBps);

    const auto cooldownEstimate = controller.update(overuseEstimate->targetBitrateBps,
                                                    std::list<TransportCcReport> {
                                                        makeReport(19'000, 10, 0, 10'000, 10'000, 100'000)});
    CPPUNIT_ASSERT(cooldownEstimate);
    CPPUNIT_ASSERT_EQUAL(overuseEstimate->targetBitrateBps, cooldownEstimate->targetBitrateBps);
    CPPUNIT_ASSERT(cooldownEstimate->state == TransportCcBandwidthState::Hold);

    const auto recoveryEstimate = controller.update(cooldownEstimate->targetBitrateBps,
                                                    std::list<TransportCcReport> {
                                                        makeReport(19'100, 10, 0, 10'000, 10'000, 200'000)});

    CPPUNIT_ASSERT(recoveryEstimate);
    CPPUNIT_ASSERT_EQUAL(uint64_t(952'000), recoveryEstimate->targetBitrateBps);
    CPPUNIT_ASSERT(recoveryEstimate->state == TransportCcBandwidthState::Increase);
}

void
TransportCcControllerTest::overuseThenCleanFeedbackReportsRecovery()
{
    TransportCcController controller;
    const auto delayReport = makeReport(19'000, 10, 0, 10'000, 35'000);

    controller.update(1'000'000, std::list<TransportCcReport> {delayReport});
    const auto overuseEstimate = controller.update(1'000'000, std::list<TransportCcReport> {delayReport});
    CPPUNIT_ASSERT(overuseEstimate);
    CPPUNIT_ASSERT(overuseEstimate->estimatorState == TransportCcEstimatorState::Overusing);

    const auto cleanEstimate = controller.update(overuseEstimate->targetBitrateBps,
                                                 std::list<TransportCcReport> {makeReport(19'100, 10, 0, 5'000, 5'000)});

    CPPUNIT_ASSERT(cleanEstimate);
    CPPUNIT_ASSERT(cleanEstimate->estimatorState == TransportCcEstimatorState::Recovering);
    CPPUNIT_ASSERT(cleanEstimate->state == TransportCcBandwidthState::Hold);

    const auto nextCleanEstimate = controller.update(cleanEstimate->targetBitrateBps,
                                                     std::list<TransportCcReport> {
                                                         makeReport(19'200, 10, 0, 5'000, 5'000, 100'000)});
    CPPUNIT_ASSERT(nextCleanEstimate);
    CPPUNIT_ASSERT(nextCleanEstimate->state == TransportCcBandwidthState::Increase);
}

void
TransportCcControllerTest::negativeDelayTrendReportsUnderusing()
{
    TransportCcControllerConfig config;
    config.delayTrendSmoothingFactor = 0.0;
    TransportCcController controller(config);
    const auto report = makeReport(20'000, 10, 0, 20'000, 5'000);

    const auto estimate = controller.update(500'000, std::list<TransportCcReport> {report});

    CPPUNIT_ASSERT(estimate);
    CPPUNIT_ASSERT(estimate->delayTrendUs < config.underuseTrendThresholdUs);
    CPPUNIT_ASSERT(estimate->estimatorState == TransportCcEstimatorState::Underusing);
}

void
TransportCcControllerTest::queueDelayTracksMinimumDelayBaseline()
{
    TransportCcControllerConfig config;
    config.delayTrendSmoothingFactor = 0.0;
    TransportCcController controller(config);

    const auto cleanEstimate = controller.update(800'000,
                                                 std::list<TransportCcReport> {
                                                     makeReport(21'000, 10, 0, 10'000, 10'000, 0)});
    CPPUNIT_ASSERT(cleanEstimate);
    CPPUNIT_ASSERT_EQUAL(int64_t(0), cleanEstimate->queueDelayUs);

    const auto queueEstimate = controller.update(800'000,
                                                 std::list<TransportCcReport> {
                                                     makeReport(21'100, 10, 0, 10'000, 35'000, 200'000)});
    CPPUNIT_ASSERT(queueEstimate);
    CPPUNIT_ASSERT(queueEstimate->queueDelayUs > 100'000);

    const auto drainEstimate = controller.update(queueEstimate->targetBitrateBps,
                                                 std::list<TransportCcReport> {
                                                     makeReport(21'200, 10, 0, 35'000, 10'000, 700'000)});
    CPPUNIT_ASSERT(drainEstimate);
    CPPUNIT_ASSERT(drainEstimate->queueDelayUs < queueEstimate->queueDelayUs);
}

void
TransportCcControllerTest::isolatedLossIsClassifiedAsRandom()
{
    TransportCcController controller;
    const auto estimate = controller.update(1'000'000,
                                            std::list<TransportCcReport> {makeReport(22'000, 10, 5, 10'000, 10'000)});

    CPPUNIT_ASSERT(estimate);
    CPPUNIT_ASSERT_EQUAL(size_t(1), estimate->maxConsecutiveLostPackets);
    CPPUNIT_ASSERT(estimate->lossType == TransportCcLossType::Random);
}

void
TransportCcControllerTest::consecutiveLossIsClassifiedAsBurst()
{
    TransportCcController controller;
    const auto estimate = controller.update(1'000'000,
                                            std::list<TransportCcReport> {makeBurstLossReport(23'000, 12, 4, 3)});

    CPPUNIT_ASSERT(estimate);
    CPPUNIT_ASSERT_EQUAL(size_t(3), estimate->maxConsecutiveLostPackets);
    CPPUNIT_ASSERT(estimate->lossType == TransportCcLossType::Burst);
}

void
TransportCcControllerTest::delayedLossIsClassifiedAsCongestion()
{
    TransportCcControllerConfig config;
    config.delayTrendSmoothingFactor = 0.0;
    TransportCcController controller(config);
    const auto estimate = controller.update(1'000'000,
                                            std::list<TransportCcReport> {makeReport(24'000, 10, 5, 10'000, 35'000)});

    CPPUNIT_ASSERT(estimate);
    CPPUNIT_ASSERT(estimate->queueDelayUs >= config.congestionQueueDelayThresholdUs);
    CPPUNIT_ASSERT(estimate->lossType == TransportCcLossType::Congestion);
}

void
TransportCcControllerTest::missingFeedbackReportsAreTracked()
{
    TransportCcController controller;
    auto firstReport = makeReport(25'000, 10, 0, 10'000, 10'000, 0);
    auto secondReport = makeReport(25'100, 10, 0, 10'000, 10'000, 200'000);
    firstReport.feedback.feedbackPacketCount = 10;
    secondReport.feedback.feedbackPacketCount = 13;

    controller.update(700'000, std::list<TransportCcReport> {firstReport});
    const auto estimate = controller.update(700'000, std::list<TransportCcReport> {secondReport});

    CPPUNIT_ASSERT(estimate);
    CPPUNIT_ASSERT_EQUAL(size_t(2), estimate->missingFeedbackReports);
    CPPUNIT_ASSERT_EQUAL(size_t(0), estimate->reorderedFeedbackReports);
    CPPUNIT_ASSERT(estimate->feedbackState == TransportCcFeedbackState::Missing);
}

void
TransportCcControllerTest::reorderedFeedbackReportsAreTracked()
{
    TransportCcController controller;
    auto firstReport = makeReport(26'000, 10, 0, 10'000, 10'000, 0);
    auto secondReport = makeReport(26'100, 10, 0, 10'000, 10'000, 200'000);
    firstReport.feedback.feedbackPacketCount = 20;
    secondReport.feedback.feedbackPacketCount = 19;

    controller.update(700'000, std::list<TransportCcReport> {firstReport});
    const auto estimate = controller.update(700'000, std::list<TransportCcReport> {secondReport});

    CPPUNIT_ASSERT(estimate);
    CPPUNIT_ASSERT_EQUAL(size_t(0), estimate->missingFeedbackReports);
    CPPUNIT_ASSERT_EQUAL(size_t(1), estimate->reorderedFeedbackReports);
    CPPUNIT_ASSERT(estimate->feedbackState == TransportCcFeedbackState::Reordered);
}

void
TransportCcControllerTest::feedbackCounterWrapIsHandled()
{
    TransportCcController controller;
    auto firstReport = makeReport(27'000, 10, 0, 10'000, 10'000, 0);
    auto secondReport = makeReport(27'100, 10, 0, 10'000, 10'000, 200'000);
    firstReport.feedback.feedbackPacketCount = 255;
    secondReport.feedback.feedbackPacketCount = 0;

    controller.update(700'000, std::list<TransportCcReport> {firstReport});
    const auto estimate = controller.update(700'000, std::list<TransportCcReport> {secondReport});

    CPPUNIT_ASSERT(estimate);
    CPPUNIT_ASSERT_EQUAL(size_t(0), estimate->missingFeedbackReports);
    CPPUNIT_ASSERT_EQUAL(size_t(0), estimate->reorderedFeedbackReports);
    CPPUNIT_ASSERT(estimate->feedbackState == TransportCcFeedbackState::Normal);
}

void
TransportCcControllerTest::feedbackCounterResetDoesNotPoisonNextReport()
{
    TransportCcController controller;
    auto firstReport = makeReport(27'500, 10, 0, 10'000, 10'000, 0);
    auto resetReport = makeReport(27'600, 10, 0, 10'000, 10'000, 200'000);
    auto nextReport = makeReport(27'700, 10, 0, 10'000, 10'000, 400'000);
    firstReport.feedback.feedbackPacketCount = 200;
    resetReport.feedback.feedbackPacketCount = 10;
    nextReport.feedback.feedbackPacketCount = 11;

    controller.update(700'000, std::list<TransportCcReport> {firstReport});
    const auto resetEstimate = controller.update(700'000, std::list<TransportCcReport> {resetReport});
    const auto nextEstimate = controller.update(700'000, std::list<TransportCcReport> {nextReport});

    CPPUNIT_ASSERT(resetEstimate);
    CPPUNIT_ASSERT_EQUAL(size_t(1), resetEstimate->reorderedFeedbackReports);
    CPPUNIT_ASSERT(resetEstimate->feedbackState == TransportCcFeedbackState::Reordered);
    CPPUNIT_ASSERT(nextEstimate);
    CPPUNIT_ASSERT_EQUAL(size_t(0), nextEstimate->missingFeedbackReports);
    CPPUNIT_ASSERT_EQUAL(size_t(0), nextEstimate->reorderedFeedbackReports);
    CPPUNIT_ASSERT(nextEstimate->feedbackState == TransportCcFeedbackState::Normal);
}

void
TransportCcControllerTest::feedbackMediaSsrcChangeResetsEstimatorState()
{
    TransportCcController controller;
    auto firstReport = makeReport(27'800, 10, 0, 10'000, 10'000, 0);
    auto secondReport = makeReport(27'900, 10, 0, 10'000, 10'000, 200'000);
    firstReport.feedback.mediaSsrc = 1111;
    secondReport.feedback.mediaSsrc = 2222;

    const auto firstEstimate = controller.update(700'000, std::list<TransportCcReport> {firstReport});
    const auto secondEstimate = controller.update(700'000, std::list<TransportCcReport> {secondReport});

    CPPUNIT_ASSERT(firstEstimate);
    CPPUNIT_ASSERT(secondEstimate);
    CPPUNIT_ASSERT(firstEstimate->estimatorState == TransportCcEstimatorState::Startup);
    CPPUNIT_ASSERT(secondEstimate->estimatorState == TransportCcEstimatorState::Startup);
    CPPUNIT_ASSERT_EQUAL(size_t(0), secondEstimate->missingFeedbackReports);
    CPPUNIT_ASSERT_EQUAL(size_t(0), secondEstimate->reorderedFeedbackReports);
}

void
TransportCcControllerTest::lateFeedbackSequenceIsClassifiedAsReordered()
{
    TransportCcController controller;
    auto firstReport = makeReport(28'000, 10, 0, 10'000, 10'000, 0);
    auto lateReport = makeReport(27'900, 10, 0, 10'000, 10'000, 200'000);
    firstReport.feedback.feedbackPacketCount = 20;
    lateReport.feedback.feedbackPacketCount = 21;

    controller.update(700'000, std::list<TransportCcReport> {firstReport});
    const auto estimate = controller.update(700'000, std::list<TransportCcReport> {lateReport});

    CPPUNIT_ASSERT(estimate);
    CPPUNIT_ASSERT_EQUAL(size_t(0), estimate->missingFeedbackReports);
    CPPUNIT_ASSERT_EQUAL(size_t(1), estimate->reorderedFeedbackReports);
    CPPUNIT_ASSERT(estimate->feedbackState == TransportCcFeedbackState::Reordered);
}

void
TransportCcControllerTest::feedbackSequenceWrapIsHandled()
{
    TransportCcController controller;
    auto firstReport = makeReport(65'530, 5, 0, 10'000, 10'000, 0);
    auto wrappedReport = makeReport(4, 10, 0, 10'000, 10'000, 100'000);
    firstReport.feedback.feedbackPacketCount = 30;
    wrappedReport.feedback.feedbackPacketCount = 31;

    controller.update(700'000, std::list<TransportCcReport> {firstReport});
    const auto estimate = controller.update(700'000, std::list<TransportCcReport> {wrappedReport});

    CPPUNIT_ASSERT(estimate);
    CPPUNIT_ASSERT_EQUAL(size_t(0), estimate->missingFeedbackReports);
    CPPUNIT_ASSERT_EQUAL(size_t(0), estimate->reorderedFeedbackReports);
    CPPUNIT_ASSERT(estimate->feedbackState == TransportCcFeedbackState::Normal);
}

void
TransportCcControllerTest::confidenceIncreasesWithPacketCount()
{
    TransportCcControllerConfig config;
    config.highConfidencePacketCount = 20;
    TransportCcController lowConfidenceController(config);
    TransportCcController highConfidenceController(config);

    const auto lowConfidenceEstimate = lowConfidenceController.update(700'000,
                                                                      std::list<TransportCcReport> {
                                                                          makeReport(28'000, 5, 0, 10'000, 10'000)});
    const auto highConfidenceEstimate = highConfidenceController.update(700'000,
                                                                        std::list<TransportCcReport> {
                                                                            makeReport(28'100, 20, 0, 10'000, 10'000)});

    CPPUNIT_ASSERT(lowConfidenceEstimate);
    CPPUNIT_ASSERT(highConfidenceEstimate);
    CPPUNIT_ASSERT(lowConfidenceEstimate->confidence < highConfidenceEstimate->confidence);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(1.0, highConfidenceEstimate->confidence, 0.001);
}

void
TransportCcControllerTest::missingFeedbackLowersConfidence()
{
    TransportCcControllerConfig config;
    config.highConfidencePacketCount = 10;
    TransportCcController normalController(config);
    TransportCcController missingController(config);

    auto firstNormalReport = makeReport(29'000, 10, 0, 10'000, 10'000, 0);
    auto secondNormalReport = makeReport(29'100, 10, 0, 10'000, 10'000, 200'000);
    firstNormalReport.feedback.feedbackPacketCount = 1;
    secondNormalReport.feedback.feedbackPacketCount = 2;
    normalController.update(700'000, std::list<TransportCcReport> {firstNormalReport});
    const auto normalEstimate = normalController.update(700'000, std::list<TransportCcReport> {secondNormalReport});

    auto firstMissingReport = makeReport(29'200, 10, 0, 10'000, 10'000, 0);
    auto secondMissingReport = makeReport(29'300, 10, 0, 10'000, 10'000, 200'000);
    firstMissingReport.feedback.feedbackPacketCount = 1;
    secondMissingReport.feedback.feedbackPacketCount = 3;
    missingController.update(700'000, std::list<TransportCcReport> {firstMissingReport});
    const auto missingEstimate = missingController.update(700'000, std::list<TransportCcReport> {secondMissingReport});

    CPPUNIT_ASSERT(normalEstimate);
    CPPUNIT_ASSERT(missingEstimate);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(1.0, normalEstimate->confidence, 0.001);
    CPPUNIT_ASSERT(missingEstimate->confidence < normalEstimate->confidence);
}

void
TransportCcControllerTest::staleFeedbackLowersConfidence()
{
    TransportCcControllerConfig config;
    config.highConfidencePacketCount = 10;
    config.staleFeedbackAgeUs = 1'000'000;
    config.staleFeedbackConfidencePenalty = 0.50;
    TransportCcController freshController(config);
    TransportCcController staleController(config);

    auto freshReport = makeReport(29'500, 10, 0, 10'000, 10'000);
    auto staleReport = makeReport(29'600, 10, 0, 10'000, 10'000);
    const auto nowUs = steadyNowUs();
    freshReport.feedbackReceiveTimeUs = nowUs;
    staleReport.feedbackReceiveTimeUs = nowUs - 2'000'000;

    const auto freshEstimate = freshController.update(700'000, std::list<TransportCcReport> {freshReport});
    const auto staleEstimate = staleController.update(700'000, std::list<TransportCcReport> {staleReport});

    CPPUNIT_ASSERT(freshEstimate);
    CPPUNIT_ASSERT(staleEstimate);
    CPPUNIT_ASSERT(staleEstimate->feedbackAgeUs >= config.staleFeedbackAgeUs);
    CPPUNIT_ASSERT(staleEstimate->confidence < freshEstimate->confidence);
}

void
TransportCcControllerTest::missingSendHistoryLowersConfidence()
{
    TransportCcControllerConfig config;
    config.highConfidencePacketCount = 10;
    config.missingSendHistoryConfidencePenalty = 0.50;
    TransportCcController completeController(config);
    TransportCcController missingHistoryController(config);

    auto completeReport = makeReport(29'700, 10, 0, 10'000, 10'000);
    auto missingHistoryReport = makeReport(29'800, 10, 0, 10'000, 10'000);
    missingHistoryReport.missingSendHistoryPackets = 3;

    const auto completeEstimate = completeController.update(700'000, std::list<TransportCcReport> {completeReport});
    const auto missingHistoryEstimate = missingHistoryController.update(700'000,
                                                                        std::list<TransportCcReport> {
                                                                            missingHistoryReport});

    CPPUNIT_ASSERT(completeEstimate);
    CPPUNIT_ASSERT(missingHistoryEstimate);
    CPPUNIT_ASSERT_EQUAL(size_t(3), missingHistoryEstimate->missingSendHistoryPackets);
    CPPUNIT_ASSERT(missingHistoryEstimate->confidence < completeEstimate->confidence);
}

void
TransportCcControllerTest::delayTrendOutlierIsIgnored()
{
    TransportCcControllerConfig config;
    config.delayTrendSmoothingFactor = 0.0;
    config.delayOveruseCountThreshold = 1;
    config.delayTrendThresholdUs = 10'000;
    config.delayTrendOutlierThresholdUs = 50'000;
    TransportCcController controller(config);

    const auto report = makeTimedReport(30'000,
                                        {0, 10'000, 20'000, 30'000, 40'000, 50'000, 60'000, 70'000},
                                        {0, 10'000, 20'000, 530'000, 40'000, 50'000, 60'000, 70'000});
    const auto estimate = controller.update(1'000'000, std::list<TransportCcReport> {report});

    CPPUNIT_ASSERT(estimate);
    CPPUNIT_ASSERT(estimate->delayTrendUs <= config.delayTrendThresholdUs);
    CPPUNIT_ASSERT(estimate->state != TransportCcBandwidthState::Decrease);
}

void
TransportCcControllerTest::groupedDelayTrendDetectsClusteredQueueGrowth()
{
    TransportCcControllerConfig config;
    config.delayTrendSmoothingFactor = 0.0;
    config.delayOveruseCountThreshold = 1;
    config.delayTrendThresholdUs = 10'000;
    config.delayTrendGroupLengthUs = 5'000;
    config.delayTrendMinSamples = 3;
    TransportCcController controller(config);

    const auto report = makeTimedReport(31'000,
                                        {0, 1'000, 2'000, 20'000, 21'000, 22'000, 40'000, 41'000, 42'000},
                                        {0, 1'000, 2'000, 35'000, 36'000, 37'000, 70'000, 71'000, 72'000});
    const auto estimate = controller.update(1'000'000, std::list<TransportCcReport> {report});

    CPPUNIT_ASSERT(estimate);
    CPPUNIT_ASSERT(estimate->delayTrendUs > config.delayTrendThresholdUs);
    CPPUNIT_ASSERT(estimate->state == TransportCcBandwidthState::Decrease);
    CPPUNIT_ASSERT(estimate->reason == TransportCcEstimateReason::DelayIncrease);
}

void
TransportCcControllerTest::lowConfidenceFeedbackDoesNotIncreaseTarget()
{
    TransportCcControllerConfig config;
    config.highConfidencePacketCount = 20;
    config.minConfidenceForIncrease = 0.75;
    TransportCcController controller(config);

    const auto estimate = controller.update(700'000,
                                            std::list<TransportCcReport> {makeReport(32'000, 5, 0, 10'000, 10'000)});

    CPPUNIT_ASSERT(estimate);
    CPPUNIT_ASSERT(estimate->acknowledgedBitrateBps > 700'000);
    CPPUNIT_ASSERT(estimate->confidence < config.minConfidenceForIncrease);
    CPPUNIT_ASSERT_EQUAL(uint64_t(700'000), estimate->targetBitrateBps);
    CPPUNIT_ASSERT(estimate->state == TransportCcBandwidthState::Hold);
}

void
TransportCcControllerTest::smallRandomLossDoesNotReduceTarget()
{
    TransportCcController controller;
    const auto estimate = controller.update(1'000'000,
                                            std::list<TransportCcReport> {makeReport(33'000, 20, 20, 10'000, 10'000)});

    CPPUNIT_ASSERT(estimate);
    CPPUNIT_ASSERT(estimate->lossType == TransportCcLossType::Random);
    CPPUNIT_ASSERT(estimate->packetLossRatio < 0.10);
    CPPUNIT_ASSERT_EQUAL(uint64_t(1'000'000), estimate->targetBitrateBps);
    CPPUNIT_ASSERT(estimate->state == TransportCcBandwidthState::Hold);
}

void
TransportCcControllerTest::queueDelayOveruseReducesTarget()
{
    TransportCcControllerConfig config;
    config.delayTrendSmoothingFactor = 0.0;
    config.delayOveruseCountThreshold = 1;
    config.delayTrendThresholdUs = 1'000'000;
    config.congestionQueueDelayThresholdUs = 50'000;
    TransportCcController controller(config);

    const auto estimate = controller.update(1'000'000,
                                            std::list<TransportCcReport> {makeReport(34'000, 10, 0, 10'000, 35'000)});

    CPPUNIT_ASSERT(estimate);
    CPPUNIT_ASSERT(estimate->delayTrendUs < config.delayTrendThresholdUs);
    CPPUNIT_ASSERT(estimate->queueDelayUs >= config.congestionQueueDelayThresholdUs);
    CPPUNIT_ASSERT(estimate->targetBitrateBps < 1'000'000);
    CPPUNIT_ASSERT(estimate->state == TransportCcBandwidthState::Decrease);
}

void
TransportCcControllerTest::longFeedbackGapResetsDelayAndRateWindows()
{
    TransportCcControllerConfig config;
    config.delayTrendSmoothingFactor = 0.0;
    config.delayOveruseCountThreshold = 1;
    config.estimatorResetIntervalUs = 1'000'000;
    TransportCcController controller(config);

    const auto queueEstimate = controller.update(800'000,
                                                 std::list<TransportCcReport> {
                                                     makeReport(35'000, 10, 0, 10'000, 35'000)});
    CPPUNIT_ASSERT(queueEstimate);
    CPPUNIT_ASSERT(queueEstimate->queueDelayUs > 0);

    const auto gapEstimate = controller.update(queueEstimate->targetBitrateBps,
                                               std::list<TransportCcReport> {
                                                   makeReport(35'100, 10, 0, 10'000, 10'000, 5'000'000)});

    CPPUNIT_ASSERT(gapEstimate);
    CPPUNIT_ASSERT_EQUAL(int64_t(0), gapEstimate->queueDelayUs);
    CPPUNIT_ASSERT_EQUAL(int64_t(0), gapEstimate->delayTrendUs);
    CPPUNIT_ASSERT(gapEstimate->acknowledgedBitrateBps > 0);
}

void
TransportCcControllerTest::simulatedCapacityDropAndRecoveryAdaptsTarget()
{
    TransportCcControllerConfig config;
    config.delayTrendSmoothingFactor = 0.0;
    config.delayTrendThresholdUs = 5'000;
    config.delayOveruseCountThreshold = 1;
    config.startupProbeCount = 1;
    TransportCcController controller(config);

    uint64_t targetBitrateBps = 500'000;
    int64_t receiveStartUs = 0;
    uint16_t sequenceNumber = 36'000;
    auto runRound = [&](uint64_t capacityBps) {
        const auto deliveredBitrateBps = std::min(targetBitrateBps, capacityBps);
        const auto report = makeCapacityReport(sequenceNumber, targetBitrateBps, capacityBps, receiveStartUs);
        sequenceNumber = static_cast<uint16_t>(sequenceNumber + 100);
        receiveStartUs += packetIntervalUsForBitrate(deliveredBitrateBps) * 20;
        const auto estimate = controller.update(targetBitrateBps, std::list<TransportCcReport> {report});
        CPPUNIT_ASSERT(estimate);
        targetBitrateBps = estimate->targetBitrateBps;
        return estimate;
    };

    for (auto round = 0; round < 4; ++round)
        runRound(1'500'000);
    const auto rampedBitrateBps = targetBitrateBps;
    CPPUNIT_ASSERT(rampedBitrateBps > 500'000);

    std::optional<TransportCcControllerEstimate> dropEstimate;
    for (auto round = 0; round < 3; ++round)
        dropEstimate = runRound(400'000);
    CPPUNIT_ASSERT(dropEstimate);
    CPPUNIT_ASSERT(dropEstimate->state == TransportCcBandwidthState::Decrease);
    const auto reducedBitrateBps = targetBitrateBps;
    CPPUNIT_ASSERT(reducedBitrateBps < rampedBitrateBps);

    for (auto round = 0; round < 6; ++round)
        runRound(1'500'000);
    CPPUNIT_ASSERT(targetBitrateBps > reducedBitrateBps);
}

void
TransportCcControllerTest::simulatedQueueGrowthTriggersDecrease()
{
    TransportCcControllerConfig config;
    config.delayTrendSmoothingFactor = 0.0;
    config.delayTrendThresholdUs = 5'000;
    config.delayOveruseCountThreshold = 1;
    TransportCcController controller(config);

    uint64_t targetBitrateBps = 900'000;
    int64_t receiveStartUs = 0;
    uint16_t sequenceNumber = 37'000;
    std::optional<TransportCcControllerEstimate> estimate;
    bool sawDecrease = false;
    int64_t maxQueueDelayUs = 0;
    for (auto round = 0; round < 3; ++round) {
        const auto capacityBps = 500'000ULL;
        const auto deliveredBitrateBps = std::min(targetBitrateBps, capacityBps);
        const auto report = makeCapacityReport(sequenceNumber, targetBitrateBps, capacityBps, receiveStartUs);
        sequenceNumber = static_cast<uint16_t>(sequenceNumber + 100);
        receiveStartUs += packetIntervalUsForBitrate(deliveredBitrateBps) * 20;
        estimate = controller.update(targetBitrateBps, std::list<TransportCcReport> {report});
        CPPUNIT_ASSERT(estimate);
        sawDecrease = sawDecrease || estimate->state == TransportCcBandwidthState::Decrease;
        maxQueueDelayUs = std::max(maxQueueDelayUs, estimate->queueDelayUs);
        targetBitrateBps = estimate->targetBitrateBps;
    }

    CPPUNIT_ASSERT(estimate);
    CPPUNIT_ASSERT(maxQueueDelayUs >= config.congestionQueueDelayThresholdUs);
    CPPUNIT_ASSERT(sawDecrease);
    CPPUNIT_ASSERT(targetBitrateBps < 900'000);
}

void
TransportCcControllerTest::simulatedCompetingTrafficKeepsTargetConservative()
{
    TransportCcControllerConfig config;
    config.delayTrendSmoothingFactor = 0.0;
    config.delayTrendThresholdUs = 5'000;
    config.delayOveruseCountThreshold = 1;
    config.startupProbeCount = 1;
    TransportCcController controller(config);

    uint64_t targetBitrateBps = 500'000;
    int64_t receiveStartUs = 0;
    uint16_t sequenceNumber = 38'000;
    auto runRound = [&](uint64_t capacityBps) {
        const auto deliveredBitrateBps = std::min(targetBitrateBps, capacityBps);
        const auto report = makeCapacityReport(sequenceNumber, targetBitrateBps, capacityBps, receiveStartUs);
        sequenceNumber = static_cast<uint16_t>(sequenceNumber + 100);
        receiveStartUs += packetIntervalUsForBitrate(deliveredBitrateBps) * 20;
        const auto estimate = controller.update(targetBitrateBps, std::list<TransportCcReport> {report});
        CPPUNIT_ASSERT(estimate);
        targetBitrateBps = estimate->targetBitrateBps;
        return estimate;
    };

    for (auto round = 0; round < 4; ++round)
        runRound(1'200'000);
    const auto rampedBitrateBps = targetBitrateBps;

    bool sawDecrease = false;
    for (auto round = 0; round < 6; ++round) {
        const auto estimate = runRound(round % 2 == 0 ? 600'000 : 500'000);
        sawDecrease = sawDecrease || estimate->state == TransportCcBandwidthState::Decrease;
    }

    CPPUNIT_ASSERT(sawDecrease);
    CPPUNIT_ASSERT(targetBitrateBps < rampedBitrateBps);
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::TransportCcControllerTest::name());