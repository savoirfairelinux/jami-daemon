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

// AES_CM_128_HMAC_SHA1_80 master key + salt (30 bytes), as carried in SDES
constexpr const char* TEST_SUITE = "AES_CM_128_HMAC_SHA1_80";
constexpr const char* TEST_PARAMS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmn";
constexpr int TAG_SIZE = 10;

struct Contexts
{
    SRTPContext sender {};
    SRTPContext receiver {};
    Contexts()
    {
        CPPUNIT_ASSERT_EQUAL(0, ff_srtp_set_crypto(&sender, TEST_SUITE, TEST_PARAMS));
        CPPUNIT_ASSERT_EQUAL(0, ff_srtp_set_crypto(&receiver, TEST_SUITE, TEST_PARAMS));
    }
    ~Contexts()
    {
        ff_srtp_free(&sender);
        ff_srtp_free(&receiver);
    }
};

std::vector<uint8_t>
rtpPacket(uint16_t seq, const std::string& payload, uint32_t ssrc = 0x11223344)
{
    std::vector<uint8_t> pkt(12, 0);
    pkt[0] = 0x80;
    pkt[1] = 96;
    pkt[2] = seq >> 8;
    pkt[3] = seq & 0xff;
    pkt[8] = ssrc >> 24;
    pkt[9] = (ssrc >> 16) & 0xff;
    pkt[10] = (ssrc >> 8) & 0xff;
    pkt[11] = ssrc & 0xff;
    pkt.insert(pkt.end(), payload.begin(), payload.end());
    return pkt;
}

// Receiver Report with a single report block (32 bytes)
std::vector<uint8_t>
rtcpRR(uint8_t fractionLost, uint32_t ssrc = 0x11223344)
{
    std::vector<uint8_t> pkt(32, 0);
    pkt[0] = 0x81;
    pkt[1] = 201;
    pkt[3] = 7; // length in 32-bit words minus one
    pkt[4] = ssrc >> 24;
    pkt[5] = (ssrc >> 16) & 0xff;
    pkt[6] = (ssrc >> 8) & 0xff;
    pkt[7] = ssrc & 0xff;
    pkt[12] = fractionLost;
    return pkt;
}

std::vector<uint8_t>
protect(SRTPContext& ctx, const std::vector<uint8_t>& plain)
{
    std::vector<uint8_t> out(plain.size() + TAG_SIZE + 4);
    auto len = ff_srtp_encrypt(&ctx, plain.data(), static_cast<int>(plain.size()), out.data(), static_cast<int>(out.size()));
    CPPUNIT_ASSERT(len > static_cast<int>(plain.size()));
    out.resize(len);
    return out;
}

// Returns the decrypted size, 0 when the packet was rejected
int
unprotect(SRTPContext& ctx, std::vector<uint8_t> pkt, int* err = nullptr)
{
    int len = static_cast<int>(pkt.size());
    auto ret = ff_srtp_decrypt(&ctx, pkt.data(), &len);
    if (err)
        *err = ret;
    if (ret < 0)
        CPPUNIT_ASSERT_EQUAL(0, len);
    return ret < 0 ? 0 : len;
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
    void rtpRoundTrip();
    void tamperedRtpYieldsNoData();
    void shortPacketsAreRejected();
    void replayedRtpIsRejected();
    void reorderedRtpWithinWindowIsAccepted();
    void rtcpRoundTripAndReplay();

    CPPUNIT_TEST_SUITE(SrtpTest);
    CPPUNIT_TEST(aes256KeyDerivationMatchesRfc6188);
    CPPUNIT_TEST(aes256CounterModeMatchesRfc6188);
    CPPUNIT_TEST(aes256ShortAuthTagSuiteIsAccepted);
    CPPUNIT_TEST(rtpRoundTrip);
    CPPUNIT_TEST(tamperedRtpYieldsNoData);
    CPPUNIT_TEST(shortPacketsAreRejected);
    CPPUNIT_TEST(replayedRtpIsRejected);
    CPPUNIT_TEST(reorderedRtpWithinWindowIsAccepted);
    CPPUNIT_TEST(rtcpRoundTripAndReplay);
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

void
SrtpTest::rtpRoundTrip()
{
    Contexts ctx;
    auto plain = rtpPacket(1, "hello");
    auto wire = protect(ctx.sender, plain);
    CPPUNIT_ASSERT_EQUAL(plain.size() + TAG_SIZE, wire.size());
    CPPUNIT_ASSERT(!std::equal(plain.begin() + 12, plain.end(), wire.begin() + 12)); // payload is encrypted

    auto received = wire;
    int len = static_cast<int>(received.size());
    CPPUNIT_ASSERT_EQUAL(0, ff_srtp_decrypt(&ctx.receiver, received.data(), &len));
    CPPUNIT_ASSERT_EQUAL(static_cast<int>(plain.size()), len);
    CPPUNIT_ASSERT(std::equal(plain.begin(), plain.end(), received.begin()));
}

void
SrtpTest::tamperedRtpYieldsNoData()
{
    Contexts ctx;
    auto wire = protect(ctx.sender, rtpPacket(1, "hello"));

    // Flip a payload bit: the tag no longer matches
    auto tampered = wire;
    tampered[13] ^= 0x01;
    int err = 0;
    CPPUNIT_ASSERT_EQUAL(0, unprotect(ctx.receiver, tampered, &err));
    CPPUNIT_ASSERT(err < 0);

    // Plain RTP forged without the key
    CPPUNIT_ASSERT_EQUAL(0, unprotect(ctx.receiver, rtpPacket(2, "forged"), &err));
    CPPUNIT_ASSERT(err < 0);

    // A rejected packet must not have moved the receiver state: the genuine one still decrypts
    CPPUNIT_ASSERT_EQUAL(static_cast<int>(wire.size() - TAG_SIZE), unprotect(ctx.receiver, wire));
}

void
SrtpTest::shortPacketsAreRejected()
{
    Contexts ctx;
    for (size_t size = 0; size < 12 + TAG_SIZE; ++size) {
        std::vector<uint8_t> pkt(size, 0);
        if (size > 1)
            pkt[1] = 96;
        int err = 0;
        CPPUNIT_ASSERT_EQUAL(0, unprotect(ctx.receiver, pkt, &err));
        CPPUNIT_ASSERT(err < 0);
    }
    // RTCP: header + index + tag
    for (size_t size = 2; size < 8 + 4 + TAG_SIZE; ++size) {
        std::vector<uint8_t> pkt(size, 0);
        pkt[1] = 201;
        int err = 0;
        CPPUNIT_ASSERT_EQUAL(0, unprotect(ctx.receiver, pkt, &err));
        CPPUNIT_ASSERT(err < 0);
    }
}

void
SrtpTest::replayedRtpIsRejected()
{
    Contexts ctx;
    auto first = protect(ctx.sender, rtpPacket(1000, "one"));
    auto second = protect(ctx.sender, rtpPacket(1001, "two"));

    CPPUNIT_ASSERT(unprotect(ctx.receiver, first) > 0);
    CPPUNIT_ASSERT_EQUAL(0, unprotect(ctx.receiver, first)); // exact replay
    CPPUNIT_ASSERT(unprotect(ctx.receiver, second) > 0);
    CPPUNIT_ASSERT_EQUAL(0, unprotect(ctx.receiver, first)); // replay of an older packet
    CPPUNIT_ASSERT_EQUAL(0, unprotect(ctx.receiver, second));

    // Anything older than the window is dropped even if never seen before
    Contexts late;
    auto oldest = protect(late.sender, rtpPacket(0, "never delivered on time"));
    for (uint16_t seq = 1; seq < SRTP_REPLAY_WINDOW_SIZE; ++seq)
        (void) protect(late.sender, rtpPacket(seq, "x"));
    auto latest = protect(late.sender, rtpPacket(SRTP_REPLAY_WINDOW_SIZE, "latest"));
    CPPUNIT_ASSERT(unprotect(late.receiver, latest) > 0);
    CPPUNIT_ASSERT_EQUAL(0, unprotect(late.receiver, oldest));
}

void
SrtpTest::reorderedRtpWithinWindowIsAccepted()
{
    Contexts ctx;
    std::vector<std::vector<uint8_t>> wire;
    for (uint16_t seq = 10; seq < 20; ++seq)
        wire.emplace_back(protect(ctx.sender, rtpPacket(seq, "p" + std::to_string(seq))));

    // Deliver out of order: every packet is new, all must pass
    for (auto i : {5, 2, 9, 0, 7, 1, 8, 3, 6, 4})
        CPPUNIT_ASSERT(unprotect(ctx.receiver, wire[i]) > 0);
    // …and none of them twice
    for (const auto& pkt : wire)
        CPPUNIT_ASSERT_EQUAL(0, unprotect(ctx.receiver, pkt));
}

void
SrtpTest::rtcpRoundTripAndReplay()
{
    Contexts ctx;
    auto plain = rtcpRR(42);
    auto wire = protect(ctx.sender, plain);
    // SRTCP appends the 4-byte index and the tag; the payload is encrypted (E bit set)
    CPPUNIT_ASSERT_EQUAL(plain.size() + 4 + TAG_SIZE, wire.size());
    CPPUNIT_ASSERT_EQUAL(0x80, wire[plain.size()] & 0x80);
    CPPUNIT_ASSERT(!std::equal(plain.begin() + 8, plain.end(), wire.begin() + 8));

    auto received = wire;
    int len = static_cast<int>(received.size());
    CPPUNIT_ASSERT_EQUAL(0, ff_srtp_decrypt(&ctx.receiver, received.data(), &len));
    CPPUNIT_ASSERT_EQUAL(static_cast<int>(plain.size()), len);
    CPPUNIT_ASSERT(std::equal(plain.begin(), plain.end(), received.begin()));

    // Replay and forgery are rejected
    int err = 0;
    CPPUNIT_ASSERT_EQUAL(0, unprotect(ctx.receiver, wire, &err));
    CPPUNIT_ASSERT(err < 0);
    CPPUNIT_ASSERT_EQUAL(0, unprotect(ctx.receiver, rtcpRR(255), &err));
    CPPUNIT_ASSERT(err < 0);

    // The next genuine report is still accepted
    CPPUNIT_ASSERT_EQUAL(static_cast<int>(plain.size()), unprotect(ctx.receiver, protect(ctx.sender, rtcpRR(43))));
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::SrtpTest::name());
