#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <array>
#include <cstdint>
#include <stdexcept>
#include <tuple>
#include <utility>

#include <fmt/format.h>

#include "media/dtls_srtp.h"
#include "media/socket_pair.h"
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
    void dtlsContextIsReusableForIceRestart();
    void fingerprintsSupportSha384AndSha512();
    void unsupportedFingerprintHashIsRejected();
    void bundleCacheChecksDtlsBinding();

    CPPUNIT_TEST_SUITE(DtlsDemuxTest);
    CPPUNIT_TEST(dtlsRecordsAreAccepted);
    CPPUNIT_TEST(srtpAndStunPacketsAreRejected);
    CPPUNIT_TEST(dtlsContextIsReusableForIceRestart);
    CPPUNIT_TEST(fingerprintsSupportSha384AndSha512);
    CPPUNIT_TEST(unsupportedFingerprintHashIsRejected);
    CPPUNIT_TEST(bundleCacheChecksDtlsBinding);
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

void
DtlsDemuxTest::dtlsContextIsReusableForIceRestart()
{
    DtlsSrtpSession session {
        DtlsSetup::PASSIVE,
        "SHA-256",
        "AA:BB:CC",
        {"SRTP_AES128_CM_SHA1_80", "outbound", "inbound"},
    };

    CPPUNIT_ASSERT(session.matches(DtlsSetup::PASSIVE, "sha-256", "AA:BB:CC"));
    CPPUNIT_ASSERT(!session.matches(DtlsSetup::ACTIVE, "sha-256", "AA:BB:CC"));
    CPPUNIT_ASSERT(!session.matches(DtlsSetup::PASSIVE, "sha-256", "DD:EE:FF"));
}

void
DtlsDemuxTest::bundleCacheChecksDtlsBinding()
{
    auto bundle = SocketPair::createBundleContext(nullptr, nullptr, true, false);
    const DtlsSrtpSession session {
        DtlsSetup::PASSIVE,
        "SHA-256",
        "AA:BB:CC",
        {"SRTP_AES128_CM_SHA1_80", "outbound", "inbound"},
    };
    SocketPair::setBundleDtlsContext(bundle, session);
    const auto context
        = SocketPair::ensureBundleDtlsContext(bundle, DtlsSetup::PASSIVE, "sha-256", "aa:bb:cc", {}, {}, {});
    CPPUNIT_ASSERT_EQUAL(session.context.outboundKeyInfo, context.outboundKeyInfo);
    CPPUNIT_ASSERT_EQUAL(session.context.inboundKeyInfo, context.inboundKeyInfo);
    auto restart = session;
    restart.context.outboundKeyInfo = "different";
    CPPUNIT_ASSERT_EQUAL(session.context.outboundKeyInfo,
                         SocketPair::setBundleDtlsContext(bundle, restart).outboundKeyInfo);
    for (const auto& [setup, hash, fingerprint] : {
             std::tuple {DtlsSetup::ACTIVE, "SHA-256", "AA:BB:CC"},
             std::tuple {DtlsSetup::PASSIVE, "SHA-384", "AA:BB:CC"},
             std::tuple {DtlsSetup::PASSIVE, "SHA-256", "DD:EE:FF"},
         }) {
        CPPUNIT_ASSERT_THROW(SocketPair::ensureBundleDtlsContext(bundle, setup, hash, fingerprint, {}, {}, {}),
                             std::runtime_error);
        restart.localSetup = setup;
        restart.remoteFingerprintHash = hash;
        restart.remoteFingerprint = fingerprint;
        CPPUNIT_ASSERT_THROW(SocketPair::setBundleDtlsContext(bundle, restart), std::runtime_error);
    }
}

void
DtlsDemuxTest::fingerprintsSupportSha384AndSha512()
{
    const auto identity = generateDtlsSrtpIdentity();
    for (const auto& [hash, digest] : {std::pair {"sha-384", GNUTLS_DIG_SHA384},
                                     std::pair {"sHa-512", GNUTLS_DIG_SHA512}}) {
        std::array<unsigned char, 64> raw {};
        size_t rawSize = raw.size();
        CPPUNIT_ASSERT_EQUAL(GNUTLS_E_SUCCESS,
                             gnutls_x509_crt_get_fingerprint(identity.second->cert,
                                                             digest,
                                                             raw.data(),
                                                             &rawSize));
        const auto fingerprint = getDtlsFingerprint(*identity.second, hash);
        CPPUNIT_ASSERT_EQUAL(rawSize * 3 - 1, fingerprint.size());
        for (size_t i = 0; i < rawSize; ++i) {
            CPPUNIT_ASSERT_EQUAL(fmt::format("{:02X}", raw[i]), fingerprint.substr(i * 3, 2));
            if (i != 0)
                CPPUNIT_ASSERT_EQUAL(':', fingerprint[i * 3 - 1]);
        }
    }
}

void
DtlsDemuxTest::unsupportedFingerprintHashIsRejected()
{
    const auto identity = generateDtlsSrtpIdentity();
    CPPUNIT_ASSERT_THROW(getDtlsFingerprint(*identity.second, "unknown"), std::runtime_error);
}

} // namespace test
} // namespace jami

JAMI_TEST_RUNNER(jami::test::DtlsDemuxTest::name());
