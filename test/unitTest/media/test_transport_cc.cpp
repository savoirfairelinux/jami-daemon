#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <array>
#include <cstdint>
#include <vector>

#include "media/transport_cc.h"
#include "test_runner.h"

namespace jami {
namespace test {
namespace {

uint16_t
readUint16(const std::vector<uint8_t>& packet, size_t offset)
{
    return (uint16_t(packet[offset]) << 8) | uint16_t(packet[offset + 1]);
}

uint32_t
readUint32(const std::vector<uint8_t>& packet, size_t offset)
{
    return (uint32_t(packet[offset]) << 24) | (uint32_t(packet[offset + 1]) << 16) | (uint32_t(packet[offset + 2]) << 8)
           | uint32_t(packet[offset + 3]);
}

void
appendUint16(std::vector<uint8_t>& packet, uint16_t value)
{
    packet.emplace_back(static_cast<uint8_t>(value >> 8));
    packet.emplace_back(static_cast<uint8_t>(value & 0xff));
}

void
appendUint32(std::vector<uint8_t>& packet, uint32_t value)
{
    packet.emplace_back(static_cast<uint8_t>(value >> 24));
    packet.emplace_back(static_cast<uint8_t>((value >> 16) & 0xff));
    packet.emplace_back(static_cast<uint8_t>((value >> 8) & 0xff));
    packet.emplace_back(static_cast<uint8_t>(value & 0xff));
}

} // namespace

class TransportCcTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "transport_cc"; }

private:
    void rtpExtensionRoundTripsSequenceNumber();
    void feedbackPacketRoundTripsRunLengthChunks();
    void parserAcceptsOneBitStatusVectorChunk();
    void parserAcceptsTwoBitStatusVectorChunk();
    void parserRejectsReservedPacketStatus();
    void creatorRejectsNonSequentialFeedback();

    CPPUNIT_TEST_SUITE(TransportCcTest);
    CPPUNIT_TEST(rtpExtensionRoundTripsSequenceNumber);
    CPPUNIT_TEST(feedbackPacketRoundTripsRunLengthChunks);
    CPPUNIT_TEST(parserAcceptsOneBitStatusVectorChunk);
    CPPUNIT_TEST(parserAcceptsTwoBitStatusVectorChunk);
    CPPUNIT_TEST(parserRejectsReservedPacketStatus);
    CPPUNIT_TEST(creatorRejectsNonSequentialFeedback);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(TransportCcTest, TransportCcTest::name());

void
TransportCcTest::rtpExtensionRoundTripsSequenceNumber()
{
    const auto extension = createTransportCcExtension(0x4a21);

    CPPUNIT_ASSERT_EQUAL(uint8_t(0x4a), extension[0]);
    CPPUNIT_ASSERT_EQUAL(uint8_t(0x21), extension[1]);
    CPPUNIT_ASSERT_EQUAL(uint16_t(0x4a21), *parseTransportCcExtension(extension.data(), extension.size()));
    CPPUNIT_ASSERT(!parseTransportCcExtension(extension.data(), 1));
}

void
TransportCcTest::feedbackPacketRoundTripsRunLengthChunks()
{
    TransportCcFeedback feedback;
    feedback.senderSsrc = 0x12345678;
    feedback.mediaSsrc = 0x23456789;
    feedback.baseSequenceNumber = 0xfffe;
    feedback.referenceTime = 0x345678;
    feedback.feedbackPacketCount = 7;
    feedback.packets = {{0xfffe, TransportCcPacketStatus::SmallDelta, 12},
                        {0xffff, TransportCcPacketStatus::NotReceived, 0},
                        {0x0000, TransportCcPacketStatus::LargeDelta, -20}};

    const auto packet = createTransportCcFeedbackPacket(feedback);

    CPPUNIT_ASSERT_EQUAL(uint8_t(0x8f), packet[0]);
    CPPUNIT_ASSERT_EQUAL(TRANSPORT_CC_RTCP_PACKET_TYPE, packet[1]);
    CPPUNIT_ASSERT_EQUAL(uint16_t(packet.size() / 4 - 1), readUint16(packet, 2));
    CPPUNIT_ASSERT_EQUAL(feedback.senderSsrc, readUint32(packet, 4));
    CPPUNIT_ASSERT_EQUAL(feedback.mediaSsrc, readUint32(packet, 8));
    CPPUNIT_ASSERT_EQUAL(feedback.baseSequenceNumber, readUint16(packet, 12));
    CPPUNIT_ASSERT_EQUAL(uint16_t(feedback.packets.size()), readUint16(packet, 14));
    CPPUNIT_ASSERT_EQUAL(uint8_t(0x34), packet[16]);
    CPPUNIT_ASSERT_EQUAL(uint8_t(0x56), packet[17]);
    CPPUNIT_ASSERT_EQUAL(uint8_t(0x78), packet[18]);
    CPPUNIT_ASSERT_EQUAL(feedback.feedbackPacketCount, packet[19]);

    const auto parsed = parseTransportCcFeedbackPacket(packet.data(), packet.size());
    CPPUNIT_ASSERT(parsed);
    CPPUNIT_ASSERT_EQUAL(feedback.senderSsrc, parsed->senderSsrc);
    CPPUNIT_ASSERT_EQUAL(feedback.mediaSsrc, parsed->mediaSsrc);
    CPPUNIT_ASSERT_EQUAL(feedback.referenceTime, parsed->referenceTime);
    CPPUNIT_ASSERT_EQUAL(feedback.feedbackPacketCount, parsed->feedbackPacketCount);
    CPPUNIT_ASSERT_EQUAL(feedback.packets.size(), parsed->packets.size());
    CPPUNIT_ASSERT_EQUAL(uint16_t(0xfffe), parsed->packets[0].sequenceNumber);
    CPPUNIT_ASSERT(parsed->packets[0].status == TransportCcPacketStatus::SmallDelta);
    CPPUNIT_ASSERT_EQUAL(int16_t(12), parsed->packets[0].deltaTicks);
    CPPUNIT_ASSERT_EQUAL(uint16_t(0xffff), parsed->packets[1].sequenceNumber);
    CPPUNIT_ASSERT(parsed->packets[1].status == TransportCcPacketStatus::NotReceived);
    CPPUNIT_ASSERT_EQUAL(uint16_t(0x0000), parsed->packets[2].sequenceNumber);
    CPPUNIT_ASSERT(parsed->packets[2].status == TransportCcPacketStatus::LargeDelta);
    CPPUNIT_ASSERT_EQUAL(int16_t(-20), parsed->packets[2].deltaTicks);
}

void
TransportCcTest::parserAcceptsOneBitStatusVectorChunk()
{
    std::vector<uint8_t> packet;
    packet.reserve(24);
    packet.emplace_back(0x8f);
    packet.emplace_back(TRANSPORT_CC_RTCP_PACKET_TYPE);
    appendUint16(packet, 5);
    appendUint32(packet, 0x11223344);
    appendUint32(packet, 0x55667788);
    appendUint16(packet, 0x0200);
    appendUint16(packet, 3);
    packet.emplace_back(0x00);
    packet.emplace_back(0x10);
    packet.emplace_back(0x20);
    packet.emplace_back(4);
    appendUint16(packet, 0xa800);
    packet.emplace_back(5);
    packet.emplace_back(7);

    const auto parsed = parseTransportCcFeedbackPacket(packet.data(), packet.size());

    CPPUNIT_ASSERT(parsed);
    CPPUNIT_ASSERT_EQUAL(size_t(3), parsed->packets.size());
    CPPUNIT_ASSERT(parsed->packets[0].status == TransportCcPacketStatus::SmallDelta);
    CPPUNIT_ASSERT_EQUAL(int16_t(5), parsed->packets[0].deltaTicks);
    CPPUNIT_ASSERT(parsed->packets[1].status == TransportCcPacketStatus::NotReceived);
    CPPUNIT_ASSERT(parsed->packets[2].status == TransportCcPacketStatus::SmallDelta);
    CPPUNIT_ASSERT_EQUAL(int16_t(7), parsed->packets[2].deltaTicks);
}

void
TransportCcTest::parserAcceptsTwoBitStatusVectorChunk()
{
    std::vector<uint8_t> packet;
    packet.reserve(28);
    packet.emplace_back(0x8f);
    packet.emplace_back(TRANSPORT_CC_RTCP_PACKET_TYPE);
    appendUint16(packet, 6);
    appendUint32(packet, 0x11223344);
    appendUint32(packet, 0x55667788);
    appendUint16(packet, 0x0100);
    appendUint16(packet, 4);
    packet.emplace_back(0x00);
    packet.emplace_back(0x10);
    packet.emplace_back(0x20);
    packet.emplace_back(3);
    appendUint16(packet, 0xd240);
    packet.emplace_back(4);
    appendUint16(packet, static_cast<uint16_t>(-8));
    packet.emplace_back(9);
    packet.emplace_back(0);
    packet.emplace_back(0);

    const auto parsed = parseTransportCcFeedbackPacket(packet.data(), packet.size());

    CPPUNIT_ASSERT(parsed);
    CPPUNIT_ASSERT_EQUAL(size_t(4), parsed->packets.size());
    CPPUNIT_ASSERT(parsed->packets[0].status == TransportCcPacketStatus::SmallDelta);
    CPPUNIT_ASSERT_EQUAL(int16_t(4), parsed->packets[0].deltaTicks);
    CPPUNIT_ASSERT(parsed->packets[1].status == TransportCcPacketStatus::NotReceived);
    CPPUNIT_ASSERT(parsed->packets[2].status == TransportCcPacketStatus::LargeDelta);
    CPPUNIT_ASSERT_EQUAL(int16_t(-8), parsed->packets[2].deltaTicks);
    CPPUNIT_ASSERT(parsed->packets[3].status == TransportCcPacketStatus::SmallDelta);
    CPPUNIT_ASSERT_EQUAL(int16_t(9), parsed->packets[3].deltaTicks);
}

void
TransportCcTest::parserRejectsReservedPacketStatus()
{
    std::vector<uint8_t> packet;
    packet.reserve(24);
    packet.emplace_back(0x8f);
    packet.emplace_back(TRANSPORT_CC_RTCP_PACKET_TYPE);
    appendUint16(packet, 5);
    appendUint32(packet, 0x11223344);
    appendUint32(packet, 0x55667788);
    appendUint16(packet, 0x0100);
    appendUint16(packet, 1);
    packet.emplace_back(0x00);
    packet.emplace_back(0x10);
    packet.emplace_back(0x20);
    packet.emplace_back(3);
    appendUint16(packet, 0x6001);
    packet.emplace_back(0);
    packet.emplace_back(0);

    CPPUNIT_ASSERT(!parseTransportCcFeedbackPacket(packet.data(), packet.size()));
}

void
TransportCcTest::creatorRejectsNonSequentialFeedback()
{
    TransportCcFeedback feedback;
    feedback.senderSsrc = 0x12345678;
    feedback.mediaSsrc = 0x23456789;
    feedback.baseSequenceNumber = 100;
    feedback.referenceTime = 0x345678;
    feedback.feedbackPacketCount = 7;
    feedback.packets = {{100, TransportCcPacketStatus::SmallDelta, 12}, {102, TransportCcPacketStatus::SmallDelta, 14}};

    CPPUNIT_ASSERT(createTransportCcFeedbackPacket(feedback).empty());
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::TransportCcTest::name());