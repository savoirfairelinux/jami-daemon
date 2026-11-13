#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <cstdint>
#include <cstring>
#include <list>
#include <vector>

#include "media/congestion_control.h"
#include "test_runner.h"

#ifdef ENABLE_VIDEO
#include "media/system_codec_container.h"
#include "media/video/video_rtp_session.h"
#endif

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
    void recognizesLegacyRembCommands();
#ifdef ENABLE_VIDEO
    void rembAdaptationUsesNegotiatedMode();
    void absoluteAdaptationNeverUsesLegacyCommands();
    void senderRestartPreservesDimensionsAndSsrcAfterBitrateFeedback();
#endif
    void transportCcEstimateReducesOnDelayAndLoss();
    void transportCcEstimateIncreasesOnCleanFeedback();

    CPPUNIT_TEST_SUITE(CongestionControlTest);
    CPPUNIT_TEST(createRembUsesStandardBitrateAndSsrcs);
    CPPUNIT_TEST(createRembRejectsMissingFeedbackSsrc);
    CPPUNIT_TEST(recognizesLegacyRembCommands);
#ifdef ENABLE_VIDEO
    CPPUNIT_TEST(rembAdaptationUsesNegotiatedMode);
    CPPUNIT_TEST(absoluteAdaptationNeverUsesLegacyCommands);
    CPPUNIT_TEST(senderRestartPreservesDimensionsAndSsrcAfterBitrateFeedback);
#endif
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
CongestionControlTest::recognizesLegacyRembCommands()
{
    CPPUNIT_ASSERT(legacyRembCommand(0x6803, true) == LegacyRembCommand::DECREASE);
    CPPUNIT_ASSERT(legacyRembCommand(0x7378, true) == LegacyRembCommand::INCREASE);
    CPPUNIT_ASSERT(legacyRembCommand(2'500'000, true) == LegacyRembCommand::NONE);
    CPPUNIT_ASSERT(legacyRembCommand(0x6803, false) == LegacyRembCommand::NONE);
    CPPUNIT_ASSERT(legacyRembCommand(0x7378, false) == LegacyRembCommand::NONE);
}

#ifdef ENABLE_VIDEO
void
CongestionControlTest::rembAdaptationUsesNegotiatedMode()
{
    struct Context
    {
        bool legacyPeer;
        bool localRemb;
        bool remoteRemb;
        bool relative;
    };
    constexpr Context contexts[] {
        {false, false, false, false},
        {false, true, true, false},
        {true, false, false, true},
        {true, true, false, true},
        {true, false, true, true},
        {true, true, true, false},
    };
    CongestionControl cc;
    video::VideoRtpSession session("remb-test", "video_0", DeviceParams {}, {});
    for (const auto& context : contexts) {
        MediaDescription local;
        MediaDescription remote;
        local.rtcp_fb_goog_remb = context.localRemb;
        remote.rtcp_fb_goog_remb = context.remoteRemb;
        session.setLegacyRembEnabled(context.legacyPeer);
        session.updateMedia(remote, local);
        for (const auto bitrateBps : {uint64_t(0x6803), uint64_t(0x7378)}) {
            const auto packet = cc.createREMB(bitrateBps, 0x12345678, {0x23456789});
            rtcpREMBHeader header {};
            std::memcpy(&header, packet.data(), sizeof(header));
            session.videoBitrateInfo_.videoBitrateCurrent = 1000;
            session.videoBitrateInfo_.videoBitrateMin = 1;
            session.videoBitrateInfo_.videoBitrateMax = 2000;
            session.lastBitrateDecrease = std::chrono::steady_clock::now() - std::chrono::minutes(1);
            session.processRembFeedback(cc.parseREMB(header));
            const unsigned expected = context.relative ? (bitrateBps == 0x6803 ? 850 : 1050)
                                                       : (bitrateBps == 0x6803 ? 27 : 30);
            CPPUNIT_ASSERT_EQUAL(expected, session.getVideoBitrateInfo().videoBitrateCurrent);
        }
    }
}

void
CongestionControlTest::absoluteAdaptationNeverUsesLegacyCommands()
{
    video::VideoRtpSession session("absolute-bitrate-test", "video_0", DeviceParams {}, {});
    session.setLegacyRembEnabled(true);
    session.videoBitrateInfo_.videoBitrateMin = 1;
    session.videoBitrateInfo_.videoBitrateMax = 2000;
    for (const auto bitrateBps : {uint64_t(0x6803), uint64_t(0x7378)}) {
        session.videoBitrateInfo_.videoBitrateCurrent = 1000;
        session.applyAbsoluteBitrate(bitrateBps);
        CPPUNIT_ASSERT_EQUAL(bitrateBps == 0x6803 ? 27u : 30u, session.getVideoBitrateInfo().videoBitrateCurrent);
    }
    session.videoBitrateInfo_.videoBitrateMin = 100;
    session.applyAbsoluteBitrate(0x6803);
    CPPUNIT_ASSERT_EQUAL(100u, session.getVideoBitrateInfo().videoBitrateCurrent);
}

void
CongestionControlTest::senderRestartPreservesDimensionsAndSsrcAfterBitrateFeedback()
{
    auto codecs = std::make_shared<SystemCodecContainer>();
    codecs->init(false);
    auto h264Codec = std::static_pointer_cast<SystemVideoCodecInfo>(codecs->searchCodecByName("H264", MEDIA_VIDEO));
    CPPUNIT_ASSERT(h264Codec);

    video::VideoRtpSession session("restart-dim-test", "video_0", DeviceParams {}, {});
    MediaDescription send;
    send.enabled = true;
    send.codec = h264Codec;
    MediaDescription receive;
    receive.codec = h264Codec;
    session.updateMedia(send, receive);

    session.seedVideoBitrate(1280 * 720);
    const auto initialSeeded = session.seededVideoBitrate_;
    CPPUNIT_ASSERT(initialSeeded > 0);
    CPPUNIT_ASSERT_EQUAL(initialSeeded, session.getVideoBitrateInfo().videoBitrateCurrent);

    // Congestion feedback updates the live target bitrate without mutating the
    // initial sizing budget used when a sender restarts for the same source.
    session.setNewBitrate(300);
    CPPUNIT_ASSERT_EQUAL(300u, session.getVideoBitrateInfo().videoBitrateCurrent);
    CPPUNIT_ASSERT_EQUAL(initialSeeded, session.seededVideoBitrate_);

    session.setNewBitrate(1300);
    CPPUNIT_ASSERT_EQUAL(1300u, session.getVideoBitrateInfo().videoBitrateCurrent);
    CPPUNIT_ASSERT_EQUAL(initialSeeded, session.seededVideoBitrate_);

    // A genuine source resolution change re-seeds the sizing budget.
    session.seedVideoBitrate(1920 * 1080);
    CPPUNIT_ASSERT(session.seededVideoBitrate_ > initialSeeded);
}
#endif

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
