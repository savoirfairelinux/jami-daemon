#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <cstdint>
#include <cstring>
#include <list>
#include <vector>

#include "media/congestion_control.h"
#include "test_runner.h"

namespace jami {
namespace test {
namespace {

uint32_t
readUint32(const std::vector<uint8_t>& packet, size_t offset)
{
    return (uint32_t(packet[offset]) << 24) | (uint32_t(packet[offset + 1]) << 16) | (uint32_t(packet[offset + 2]) << 8)
           | uint32_t(packet[offset + 3]);
}

} // namespace

class CongestionControlTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "congestion_control"; }

private:
    void createRembUsesStandardBitrateAndSsrcs();
    void createRembRejectsMissingFeedbackSsrc();
    void transportCcEstimateReducesOnDelayAndLoss();
    void transportCcEstimateIncreasesOnCleanFeedback();

    CPPUNIT_TEST_SUITE(CongestionControlTest);
    CPPUNIT_TEST(createRembUsesStandardBitrateAndSsrcs);
    CPPUNIT_TEST(createRembRejectsMissingFeedbackSsrc);
    CPPUNIT_TEST(transportCcEstimateReducesOnDelayAndLoss);
    CPPUNIT_TEST(transportCcEstimateIncreasesOnCleanFeedback);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(CongestionControlTest, CongestionControlTest::name());

void
CongestionControlTest::createRembUsesStandardBitrateAndSsrcs()
{
    CongestionControl cc;
    constexpr uint64_t bitrateBps = 2'500'000;
    constexpr uint32_t senderSsrc = 0x12345678;
    const std::vector<uint32_t> feedbackSsrcs {0x23456789, 0x3456789a};

    auto packet = cc.createREMB(bitrateBps, senderSsrc, feedbackSsrcs);

    CPPUNIT_ASSERT_EQUAL(size_t(28), packet.size());
    CPPUNIT_ASSERT_EQUAL(uint8_t(0x8f), packet[0]);
    CPPUNIT_ASSERT_EQUAL(uint8_t(206), packet[1]);
    CPPUNIT_ASSERT_EQUAL(uint16_t(6), uint16_t((packet[2] << 8) | packet[3]));
    CPPUNIT_ASSERT_EQUAL(senderSsrc, readUint32(packet, 4));
    CPPUNIT_ASSERT_EQUAL(uint32_t(0), readUint32(packet, 8));
    CPPUNIT_ASSERT_EQUAL(uint32_t(0x52454d42), readUint32(packet, 12));
    CPPUNIT_ASSERT_EQUAL(uint8_t(feedbackSsrcs.size()), packet[16]);
    CPPUNIT_ASSERT_EQUAL(feedbackSsrcs[0], readUint32(packet, 20));
    CPPUNIT_ASSERT_EQUAL(feedbackSsrcs[1], readUint32(packet, 24));

    rtcpREMBHeader header {};
    std::memcpy(&header, packet.data(), sizeof(header));
    CPPUNIT_ASSERT_EQUAL(bitrateBps, cc.parseREMB(header));
}

void
CongestionControlTest::createRembRejectsMissingFeedbackSsrc()
{
    CongestionControl cc;
    const std::vector<uint32_t> feedbackSsrcs;

    CPPUNIT_ASSERT(cc.createREMB(500'000, 0x12345678, feedbackSsrcs).empty());
}

void
CongestionControlTest::transportCcEstimateReducesOnDelayAndLoss()
{
    CongestionControl cc;
    TransportCcReport report;
    report.packets = {{100, TransportCcPacketStatus::SmallDelta, 1200, 0, 0},
                      {101, TransportCcPacketStatus::SmallDelta, 1200, 10'000, 35'000},
                      {102, TransportCcPacketStatus::NotReceived, 1200, 20'000, 0}};

    const auto estimate = cc.estimateTransportCcBitrate(1'000'000, std::list<TransportCcReport> {report});

    CPPUNIT_ASSERT(estimate);
    CPPUNIT_ASSERT(estimate->bitrateBps < 1'000'000);
    CPPUNIT_ASSERT(estimate->packetLoss > 30.0f);
    CPPUNIT_ASSERT(estimate->delayTrendUs > 0);
}

void
CongestionControlTest::transportCcEstimateIncreasesOnCleanFeedback()
{
    CongestionControl cc;
    TransportCcReport report;
    report.packets = {{200, TransportCcPacketStatus::SmallDelta, 1200, 0, 0},
                      {201, TransportCcPacketStatus::SmallDelta, 1200, 10'000, 10'000},
                      {202, TransportCcPacketStatus::SmallDelta, 1200, 20'000, 20'000},
                      {203, TransportCcPacketStatus::SmallDelta, 1200, 30'000, 30'000}};

    const auto estimate = cc.estimateTransportCcBitrate(1'000'000, std::list<TransportCcReport> {report});

    CPPUNIT_ASSERT(estimate);
    CPPUNIT_ASSERT(estimate->bitrateBps > 1'000'000);
    CPPUNIT_ASSERT_EQUAL(0.0f, estimate->packetLoss);
    CPPUNIT_ASSERT_EQUAL(int64_t(0), estimate->delayTrendUs);
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::CongestionControlTest::name());
