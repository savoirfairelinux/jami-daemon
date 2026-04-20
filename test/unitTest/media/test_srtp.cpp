#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <algorithm>
#include <array>
#include <vector>

extern "C" {
#include <libavutil/aes.h>
#include <libavutil/hmac.h>
#include "media/srtp.h"
}

#include "base64.h"
#include "test_runner.h"

namespace jami {
namespace test {
namespace {

template <size_t N, size_t M>
std::vector<uint8_t>
concat(const std::array<uint8_t, N>& first, const std::array<uint8_t, M>& second)
{
    std::vector<uint8_t> value;
    value.reserve(N + M);
    value.insert(value.end(), first.begin(), first.end());
    value.insert(value.end(), second.begin(), second.end());
    return value;
}

} // namespace

class SrtpTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "srtp"; }

private:
    void aes256KeyDerivationMatchesRfc6188();
    void aes256CounterModeMatchesRfc6188();
    void aes256ShortAuthTagSuiteIsAccepted();

    CPPUNIT_TEST_SUITE(SrtpTest);
    CPPUNIT_TEST(aes256KeyDerivationMatchesRfc6188);
    CPPUNIT_TEST(aes256CounterModeMatchesRfc6188);
    CPPUNIT_TEST(aes256ShortAuthTagSuiteIsAccepted);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(SrtpTest, SrtpTest::name());

void
SrtpTest::aes256KeyDerivationMatchesRfc6188()
{
    constexpr std::array<uint8_t, 32> masterKey = {0xf0, 0xf0, 0x49, 0x14, 0xb5, 0x13, 0xf2, 0x76,
                                                    0x3a, 0x1b, 0x1f, 0xa1, 0x30, 0xf1, 0x0e, 0x29,
                                                    0x98, 0xf6, 0xf6, 0xe4, 0x3e, 0x43, 0x09, 0xd1,
                                                    0xe6, 0x22, 0xa0, 0xe3, 0x32, 0xb9, 0xf1, 0xb6};
    constexpr std::array<uint8_t, 14> masterSalt = {0x3b, 0x04, 0x80, 0x3d, 0xe5, 0x1e, 0xe7,
                                                     0xc9, 0x64, 0x23, 0xab, 0x5b, 0x78, 0xd2};
    constexpr std::array<uint8_t, 32> expectedRtpKey = {0x5b, 0xa1, 0x06, 0x4e, 0x30, 0xec, 0x51, 0x61,
                                                         0x3c, 0xad, 0x92, 0x6c, 0x5a, 0x28, 0xef, 0x73,
                                                         0x1e, 0xc7, 0xfb, 0x39, 0x7f, 0x70, 0xa9, 0x60,
                                                         0x65, 0x3c, 0xaf, 0x06, 0x55, 0x4c, 0xd8, 0xc4};
    constexpr std::array<uint8_t, 14> expectedRtpSalt = {0xfa, 0x31, 0x79, 0x16, 0x85, 0xca, 0x44,
                                                          0x4a, 0x9e, 0x07, 0xc6, 0xc6, 0x4e, 0x93};
    constexpr std::array<uint8_t, 20> expectedRtpAuth = {0xfd, 0x9c, 0x32, 0xd3, 0x9e, 0xd5, 0xfb,
                                                          0xb5, 0xa9, 0xdc, 0x96, 0xb3, 0x08, 0x18,
                                                          0x45, 0x4d, 0x13, 0x13, 0xdc, 0x05};

    const auto params = jami::base64::encode(concat(masterKey, masterSalt));

    SRTPContext context {};
    CPPUNIT_ASSERT_EQUAL(0, ff_srtp_set_crypto(&context, "AES_256_CM_HMAC_SHA1_80", params.c_str()));
    CPPUNIT_ASSERT_EQUAL(32, context.master_key_size);
    CPPUNIT_ASSERT_EQUAL(14, context.master_salt_size);
    CPPUNIT_ASSERT_EQUAL(32, context.session_key_size);
    CPPUNIT_ASSERT_EQUAL(10, context.rtp_hmac_size);
    CPPUNIT_ASSERT_EQUAL(10, context.rtcp_hmac_size);
    CPPUNIT_ASSERT(std::equal(expectedRtpKey.begin(), expectedRtpKey.end(), context.rtp_key));
    CPPUNIT_ASSERT(std::equal(expectedRtpSalt.begin(), expectedRtpSalt.end(), context.rtp_salt));
    CPPUNIT_ASSERT(std::equal(expectedRtpAuth.begin(), expectedRtpAuth.end(), context.rtp_auth));

    ff_srtp_free(&context);
}

void
SrtpTest::aes256CounterModeMatchesRfc6188()
{
    constexpr std::array<uint8_t, 32> sessionKey = {0x57, 0xf8, 0x2f, 0xe3, 0x61, 0x3f, 0xd1, 0x70,
                                                     0xa8, 0x5e, 0xc9, 0x3c, 0x40, 0xb1, 0xf0, 0x92,
                                                     0x2e, 0xc4, 0xcb, 0x0d, 0xc0, 0x25, 0xb5, 0x82,
                                                     0x72, 0x14, 0x7c, 0xc4, 0x38, 0x94, 0x4a, 0x98};
    constexpr std::array<uint8_t, 14> sessionSalt = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6,
                                                      0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd};
    constexpr std::array<uint8_t, 48> expectedKeystream = {0x92, 0xbd, 0xd2, 0x8a, 0x93, 0xc3, 0xf5, 0x25,
                                                            0x11, 0xc6, 0x77, 0xd0, 0x8b, 0x55, 0x15, 0xa4,
                                                            0x9d, 0xa7, 0x1b, 0x23, 0x78, 0xa8, 0x54, 0xf6,
                                                            0x70, 0x50, 0x75, 0x6d, 0xed, 0x16, 0x5b, 0xac,
                                                            0x63, 0xc4, 0x86, 0x8b, 0x70, 0x96, 0xd8, 0x84,
                                                            0x21, 0xb5, 0x63, 0xb8, 0xc9, 0x4c, 0x9a, 0x31};

    SRTPContext context {};
    context.aes = av_aes_alloc();
    context.hmac = av_hmac_alloc(AV_HMAC_SHA1);

    CPPUNIT_ASSERT(context.aes != nullptr);
    CPPUNIT_ASSERT(context.hmac != nullptr);

    context.session_key_size = 32;
    context.rtp_hmac_size = 10;
    context.rtcp_hmac_size = 10;
    std::copy(sessionKey.begin(), sessionKey.end(), context.rtp_key);
    std::copy(sessionSalt.begin(), sessionSalt.end(), context.rtp_salt);

    std::array<uint8_t, 12 + 48> packetIn {};
    packetIn[0] = 0x80;
    packetIn[1] = 0x00;

    std::array<uint8_t, 12 + 48 + 10> packetOut {};
    const auto encryptedLen = ff_srtp_encrypt(&context,
                                              packetIn.data(),
                                              static_cast<int>(packetIn.size()),
                                              packetOut.data(),
                                              static_cast<int>(packetOut.size()));

    CPPUNIT_ASSERT_EQUAL(static_cast<int>(packetOut.size()), encryptedLen);
    CPPUNIT_ASSERT(std::equal(expectedKeystream.begin(), expectedKeystream.end(), packetOut.data() + 12));

    ff_srtp_free(&context);
}

void
SrtpTest::aes256ShortAuthTagSuiteIsAccepted()
{
    constexpr std::array<uint8_t, 32> masterKey = {0xf0, 0xf0, 0x49, 0x14, 0xb5, 0x13, 0xf2, 0x76,
                                                    0x3a, 0x1b, 0x1f, 0xa1, 0x30, 0xf1, 0x0e, 0x29,
                                                    0x98, 0xf6, 0xf6, 0xe4, 0x3e, 0x43, 0x09, 0xd1,
                                                    0xe6, 0x22, 0xa0, 0xe3, 0x32, 0xb9, 0xf1, 0xb6};
    constexpr std::array<uint8_t, 14> masterSalt = {0x3b, 0x04, 0x80, 0x3d, 0xe5, 0x1e, 0xe7,
                                                     0xc9, 0x64, 0x23, 0xab, 0x5b, 0x78, 0xd2};

    const auto params = jami::base64::encode(concat(masterKey, masterSalt));

    SRTPContext context {};
    CPPUNIT_ASSERT_EQUAL(0, ff_srtp_set_crypto(&context, "AES_256_CM_HMAC_SHA1_32", params.c_str()));
    CPPUNIT_ASSERT_EQUAL(4, context.rtp_hmac_size);
    CPPUNIT_ASSERT_EQUAL(10, context.rtcp_hmac_size);

    ff_srtp_free(&context);
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::SrtpTest::name());
