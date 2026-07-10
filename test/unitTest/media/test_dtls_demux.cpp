#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <array>
#include <cstdint>

#include "media/dtls_srtp.h"
#include "test_runner.h"

namespace jami {
namespace test {

class DtlsDemuxTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "dtls_demux"; }

private:
    void dtlsRecordsAreAccepted();
    void srtpAndStunPacketsAreRejected();

    CPPUNIT_TEST_SUITE(DtlsDemuxTest);
    CPPUNIT_TEST(dtlsRecordsAreAccepted);
    CPPUNIT_TEST(srtpAndStunPacketsAreRejected);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(DtlsDemuxTest, DtlsDemuxTest::name());

void
DtlsDemuxTest::dtlsRecordsAreAccepted()
{
    // RFC 5764 §5.1.2: first byte in [20..63] is DTLS.
    const std::array<uint8_t, 13> changeCipherSpec {20, 0xfe, 0xfd};
    const std::array<uint8_t, 13> alert {21, 0xfe, 0xfd};
    const std::array<uint8_t, 13> handshake {22, 0xfe, 0xfd};
    const std::array<uint8_t, 13> applicationData {23, 0xfe, 0xfd};

    CPPUNIT_ASSERT(isDtlsPacket(changeCipherSpec.data(), changeCipherSpec.size()));
    CPPUNIT_ASSERT(isDtlsPacket(alert.data(), alert.size()));
    CPPUNIT_ASSERT(isDtlsPacket(handshake.data(), handshake.size()));
    CPPUNIT_ASSERT(isDtlsPacket(applicationData.data(), applicationData.size()));
}

void
DtlsDemuxTest::srtpAndStunPacketsAreRejected()
{
    // RTP/SRTP: first byte is 128..191 (version 2).
    const std::array<uint8_t, 12> srtp {0x80, 0x6f, 0x00, 0x01};
    // RTCP/SRTCP: first byte is also in the RTP range.
    const std::array<uint8_t, 8> srtcp {0x81, 0xc8, 0x00, 0x01};
    // STUN: first byte is 0..3.
    const std::array<uint8_t, 20> stun {0x00, 0x01, 0x00, 0x00};
    // ZRTP/other: first byte 16..19 is not DTLS.
    const std::array<uint8_t, 12> zrtp {0x10, 0x00};
    // Empty packets are not DTLS.
    CPPUNIT_ASSERT(!isDtlsPacket(srtp.data(), srtp.size()));
    CPPUNIT_ASSERT(!isDtlsPacket(srtcp.data(), srtcp.size()));
    CPPUNIT_ASSERT(!isDtlsPacket(stun.data(), stun.size()));
    CPPUNIT_ASSERT(!isDtlsPacket(zrtp.data(), zrtp.size()));
    CPPUNIT_ASSERT(!isDtlsPacket(srtp.data(), 0));
}

} // namespace test
} // namespace jami

JAMI_TEST_RUNNER(jami::test::DtlsDemuxTest::name());
