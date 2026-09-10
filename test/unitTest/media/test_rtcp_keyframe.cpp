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
#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <cstdint>
#include <vector>

#include "media/socket_pair.h"
#include "media/video/video_rtp_session.h"
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

class RtcpKeyframeTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "rtcp_keyframe"; }

private:
    void createPliBuildsStandardPacket();
    void keyframeRequestDetectsPliAndFir();
    void keyframeRequestIgnoresOtherRtcp();
    void keyframeThrottleTreatsNeverAsElapsed();

    CPPUNIT_TEST_SUITE(RtcpKeyframeTest);
    CPPUNIT_TEST(createPliBuildsStandardPacket);
    CPPUNIT_TEST(keyframeRequestDetectsPliAndFir);
    CPPUNIT_TEST(keyframeRequestIgnoresOtherRtcp);
    CPPUNIT_TEST(keyframeThrottleTreatsNeverAsElapsed);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(RtcpKeyframeTest, RtcpKeyframeTest::name());

void
RtcpKeyframeTest::createPliBuildsStandardPacket()
{
    constexpr uint32_t senderSsrc = 0x12345678;
    constexpr uint32_t mediaSsrc = 0x23456789;

    const auto packet = SocketPair::createRtcpPli(senderSsrc, mediaSsrc);

    // RFC 4585 6.3.1: PSFB (PT 206) with FMT 1 and no FCI.
    CPPUNIT_ASSERT_EQUAL(size_t(12), packet.size());
    CPPUNIT_ASSERT_EQUAL(uint8_t(0x81), packet[0]);
    CPPUNIT_ASSERT_EQUAL(uint8_t(206), packet[1]);
    CPPUNIT_ASSERT_EQUAL(uint16_t(2), uint16_t((packet[2] << 8) | packet[3]));
    CPPUNIT_ASSERT_EQUAL(senderSsrc, readUint32(packet, 4));
    CPPUNIT_ASSERT_EQUAL(mediaSsrc, readUint32(packet, 8));

    CPPUNIT_ASSERT(SocketPair::isRtcpKeyframeRequest(packet.data(), packet.size()));
}

void
RtcpKeyframeTest::keyframeRequestDetectsPliAndFir()
{
    const auto pli = SocketPair::createRtcpPli(0x1, 0x2);
    CPPUNIT_ASSERT(SocketPair::isRtcpKeyframeRequest(pli.data(), pli.size()));

    // RFC 5104 4.3.1: FIR is PSFB (PT 206) with FMT 4 and an FCI entry.
    std::vector<uint8_t> fir {0x84, 206,  0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x2a, 0x00, 0x00, 0x00};
    CPPUNIT_ASSERT(SocketPair::isRtcpKeyframeRequest(fir.data(), fir.size()));
}

void
RtcpKeyframeTest::keyframeRequestIgnoresOtherRtcp()
{
    // REMB: PSFB (PT 206) with FMT 15 must not be treated as a keyframe request.
    std::vector<uint8_t> remb {0x8f, 206,  0x00, 0x06, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
                               0x52, 0x45, 0x4d, 0x42, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02};
    CPPUNIT_ASSERT(!SocketPair::isRtcpKeyframeRequest(remb.data(), remb.size()));

    // Receiver report (PT 201).
    std::vector<uint8_t> rr {0x80, 201, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01};
    CPPUNIT_ASSERT(!SocketPair::isRtcpKeyframeRequest(rr.data(), rr.size()));

    // Truncated packet.
    std::vector<uint8_t> truncated {0x81, 206};
    CPPUNIT_ASSERT(!SocketPair::isRtcpKeyframeRequest(truncated.data(), truncated.size()));
}

void
RtcpKeyframeTest::keyframeThrottleTreatsNeverAsElapsed()
{
    using clock = std::chrono::steady_clock;
    const auto now = clock::now();
    const auto interval = std::chrono::milliseconds(500);

    // time_point::min() means "never happened": naive `now - min()` overflows
    // to a negative duration and permanently swallows keyframe feedback.
    CPPUNIT_ASSERT(!video::VideoRtpSession::withinInterval(now, clock::time_point::min(), interval));

    CPPUNIT_ASSERT(video::VideoRtpSession::withinInterval(now, now - std::chrono::milliseconds(100), interval));
    CPPUNIT_ASSERT(!video::VideoRtpSession::withinInterval(now, now - std::chrono::seconds(2), interval));
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::RtcpKeyframeTest::name());
