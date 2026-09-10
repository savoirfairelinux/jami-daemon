#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <cstdint>
#include <vector>

#include "media/rtp_pacer.h"
#include "test_runner.h"

namespace jami {
namespace test {
namespace {

std::vector<uint8_t>
makePacket(size_t bytes)
{
    return std::vector<uint8_t>(bytes, 0x80);
}

} // namespace

class RtpPacerTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "rtp_pacer"; }

private:
    void spacesPacketsAtConfiguredBitrate();
    void clampsQueueDelayInsteadOfDropping();
    void clearEmptiesQueue();

    CPPUNIT_TEST_SUITE(RtpPacerTest);
    CPPUNIT_TEST(spacesPacketsAtConfiguredBitrate);
    CPPUNIT_TEST(clampsQueueDelayInsteadOfDropping);
    CPPUNIT_TEST(clearEmptiesQueue);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(RtpPacerTest, RtpPacerTest::name());

void
RtpPacerTest::spacesPacketsAtConfiguredBitrate()
{
    RtpPacer pacer({960'000, 200'000});

    CPPUNIT_ASSERT(pacer.enqueue(makePacket(1'200), 1'000));
    CPPUNIT_ASSERT(pacer.enqueue(makePacket(1'200), 1'000));
    CPPUNIT_ASSERT(pacer.enqueue(makePacket(1'200), 1'000));
    CPPUNIT_ASSERT_EQUAL(size_t(3), pacer.queuedPackets());
    CPPUNIT_ASSERT_EQUAL(size_t(3'600), pacer.queuedBytes());

    const auto firstPacket = pacer.popReady(1'000);
    CPPUNIT_ASSERT(firstPacket);
    CPPUNIT_ASSERT_EQUAL(int64_t(1'000), firstPacket->releaseTimeUs);

    CPPUNIT_ASSERT(!pacer.popReady(10'999));
    const auto secondPacket = pacer.popReady(11'000);
    CPPUNIT_ASSERT(secondPacket);
    CPPUNIT_ASSERT_EQUAL(int64_t(11'000), secondPacket->releaseTimeUs);

    CPPUNIT_ASSERT(!pacer.popReady(20'999));
    const auto thirdPacket = pacer.popReady(21'000);
    CPPUNIT_ASSERT(thirdPacket);
    CPPUNIT_ASSERT_EQUAL(int64_t(21'000), thirdPacket->releaseTimeUs);
    CPPUNIT_ASSERT(pacer.empty());
}

void
RtpPacerTest::clampsQueueDelayInsteadOfDropping()
{
    // 1200 bytes at 96 kbps = 100 ms spacing per packet.
    RtpPacer pacer({96'000, 150'000});

    // Dropping packets from the middle of an encoded frame corrupts it and
    // forces a keyframe round-trip, so bursts (typically keyframes) must be
    // kept: the queue delay is bounded by draining faster instead.
    CPPUNIT_ASSERT(pacer.enqueue(makePacket(1'200), 0));
    CPPUNIT_ASSERT(pacer.enqueue(makePacket(1'200), 0));
    CPPUNIT_ASSERT(pacer.enqueue(makePacket(1'200), 0));

    CPPUNIT_ASSERT_EQUAL(size_t(3), pacer.queuedPackets());

    const auto firstPacket = pacer.popReady(0);
    CPPUNIT_ASSERT(firstPacket);
    CPPUNIT_ASSERT_EQUAL(int64_t(0), firstPacket->releaseTimeUs);

    const auto secondPacket = pacer.popReady(100'000);
    CPPUNIT_ASSERT(secondPacket);
    CPPUNIT_ASSERT_EQUAL(int64_t(100'000), secondPacket->releaseTimeUs);

    // The third packet would have been released at 200 ms, beyond the 150 ms
    // queue delay budget: it is released early instead of dropped.
    const auto thirdPacket = pacer.popReady(150'000);
    CPPUNIT_ASSERT(thirdPacket);
    CPPUNIT_ASSERT_EQUAL(int64_t(150'000), thirdPacket->releaseTimeUs);
    CPPUNIT_ASSERT(pacer.empty());
}

void
RtpPacerTest::clearEmptiesQueue()
{
    RtpPacer pacer({96'000, 0});

    CPPUNIT_ASSERT(pacer.enqueue(makePacket(1'200), 0));
    CPPUNIT_ASSERT(pacer.enqueue(makePacket(1'200), 0));
    pacer.clear();

    CPPUNIT_ASSERT(pacer.empty());
    CPPUNIT_ASSERT_EQUAL(size_t(0), pacer.queuedBytes());
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::RtpPacerTest::name());
