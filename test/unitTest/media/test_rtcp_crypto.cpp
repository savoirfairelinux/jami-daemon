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

#include <atomic>
#include <cstdint>
#include <cstring>
#include <vector>

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <fmt/format.h>

extern "C" {
#include <libavformat/avio.h>
#include "media/srtp.h"
}

#include "media/media_io_handle.h"
#include "media/socket_pair.h"
#include "test_runner.h"

namespace jami {
namespace test {
namespace {

constexpr const char* SRTP_SUITE = "AES_CM_128_HMAC_SHA1_80";
// base64 of a 16-byte master key followed by a 14-byte master salt.
constexpr const char* SRTP_PARAMS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmn";
constexpr int SRTCP_OVERHEAD = 4 /* SRTCP index */ + 10 /* HMAC-SHA1-80 */;

class UdpSocket
{
public:
    UdpSocket()
    {
        fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
        CPPUNIT_ASSERT(fd_ >= 0);

        sockaddr_in addr {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;
        CPPUNIT_ASSERT_EQUAL(0, ::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)));

        socklen_t len = sizeof(addr);
        CPPUNIT_ASSERT_EQUAL(0, ::getsockname(fd_, reinterpret_cast<sockaddr*>(&addr), &len));
        port_ = ntohs(addr.sin_port);

        timeval timeout {2, 0};
        ::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    }

    ~UdpSocket()
    {
        if (fd_ >= 0)
            ::close(fd_);
    }

    uint16_t port() const { return port_; }

    std::vector<uint8_t> receive()
    {
        std::vector<uint8_t> buffer(2048);
        const auto received = ::recv(fd_, buffer.data(), buffer.size(), 0);
        CPPUNIT_ASSERT(received > 0);
        buffer.resize(static_cast<size_t>(received));
        return buffer;
    }

    void sendTo(uint16_t port, const std::vector<uint8_t>& packet)
    {
        sockaddr_in addr {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(port);
        CPPUNIT_ASSERT_EQUAL(static_cast<ssize_t>(packet.size()),
                             ::sendto(fd_,
                                      packet.data(),
                                      packet.size(),
                                      0,
                                      reinterpret_cast<sockaddr*>(&addr),
                                      sizeof(addr)));
    }

private:
    int fd_ {-1};
    uint16_t port_ {};
};

uint16_t
reserveEphemeralPort()
{
    // Bind then release a loopback port; the small reuse race is acceptable
    // for tests.
    UdpSocket socket;
    return socket.port();
}

std::vector<uint8_t>
protectRtcp(const std::vector<uint8_t>& packet)
{
    SRTPContext context {};
    CPPUNIT_ASSERT_EQUAL(0, ff_srtp_set_crypto(&context, SRTP_SUITE, SRTP_PARAMS));
    std::vector<uint8_t> encrypted(packet.size() + SRTCP_OVERHEAD);
    const auto encryptedSize = ff_srtp_encrypt(&context,
                                               packet.data(),
                                               static_cast<int>(packet.size()),
                                               encrypted.data(),
                                               static_cast<int>(encrypted.size()));
    ff_srtp_free(&context);
    CPPUNIT_ASSERT_EQUAL(packet.size() + SRTCP_OVERHEAD, static_cast<size_t>(encryptedSize));
    encrypted.resize(static_cast<size_t>(encryptedSize));
    return encrypted;
}

std::vector<uint8_t>
protectRtp(const std::vector<uint8_t>& packet)
{
    SRTPContext context {};
    CPPUNIT_ASSERT_EQUAL(0, ff_srtp_set_crypto(&context, SRTP_SUITE, SRTP_PARAMS));
    std::vector<uint8_t> encrypted(packet.size() + 10);
    const auto encryptedSize = ff_srtp_encrypt(&context,
                                               packet.data(),
                                               static_cast<int>(packet.size()),
                                               encrypted.data(),
                                               static_cast<int>(encrypted.size()));
    ff_srtp_free(&context);
    CPPUNIT_ASSERT_EQUAL(packet.size() + 10, static_cast<size_t>(encryptedSize));
    encrypted.resize(static_cast<size_t>(encryptedSize));
    return encrypted;
}

std::vector<uint8_t>
rtpPacket(uint16_t sequence)
{
    std::vector<uint8_t> packet(16, 0);
    packet[0] = 0x80;
    packet[1] = 96;
    packet[2] = sequence >> 8;
    packet[3] = sequence & 0xff;
    packet[8] = 0x11;
    packet[9] = 0x22;
    packet[10] = 0x33;
    packet[11] = 0x44;
    packet[12] = 0xde;
    packet[13] = 0xad;
    packet[14] = 0xbe;
    packet[15] = 0xef;
    return packet;
}

} // namespace

class RtcpCryptoTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "rtcp_crypto"; }

private:
    void srtcpRoundTripProtectsRtcpPackets();
    void rtcpProtectionEncryptsOutgoingRtcp();
    void legacyPeersStillReceivePlaintextRtcp();
    void encryptedIncomingPliTriggersKeyframeCallback();
    void plaintextIncomingPliStillTriggersKeyframeCallback();
    void corruptedSrtcpDoesNotEndLiveStream();
    void rtcpByeDoesNotEndLiveStream();
    void replayedRtpDoesNotEndStream();
    void stoppedReadIsCancelled();

    CPPUNIT_TEST_SUITE(RtcpCryptoTest);
    CPPUNIT_TEST(srtcpRoundTripProtectsRtcpPackets);
    CPPUNIT_TEST(rtcpProtectionEncryptsOutgoingRtcp);
    CPPUNIT_TEST(legacyPeersStillReceivePlaintextRtcp);
    CPPUNIT_TEST(encryptedIncomingPliTriggersKeyframeCallback);
    CPPUNIT_TEST(plaintextIncomingPliStillTriggersKeyframeCallback);
    CPPUNIT_TEST(corruptedSrtcpDoesNotEndLiveStream);
    CPPUNIT_TEST(rtcpByeDoesNotEndLiveStream);
    CPPUNIT_TEST(replayedRtpDoesNotEndStream);
    CPPUNIT_TEST(stoppedReadIsCancelled);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(RtcpCryptoTest, RtcpCryptoTest::name());

void
RtcpCryptoTest::srtcpRoundTripProtectsRtcpPackets()
{
    const auto pli = SocketPair::createRtcpPli(0x11223344, 0x55667788);
    const auto encrypted = protectRtcp(pli);

    // RFC 3711 3.4: SRTCP appends an index (with the E-bit set when the
    // payload is encrypted) and an authentication tag.
    CPPUNIT_ASSERT_EQUAL(pli.size() + SRTCP_OVERHEAD, encrypted.size());
    CPPUNIT_ASSERT(std::memcmp(encrypted.data(), pli.data(), 8) == 0); // Header stays clear
    CPPUNIT_ASSERT(std::memcmp(encrypted.data() + 8, pli.data() + 8, 4) != 0);
    CPPUNIT_ASSERT_EQUAL(uint8_t(0x80), uint8_t(encrypted[pli.size()] & 0x80)); // E-bit

    SRTPContext decryptContext {};
    CPPUNIT_ASSERT_EQUAL(0, ff_srtp_set_crypto(&decryptContext, SRTP_SUITE, SRTP_PARAMS));
    std::vector<uint8_t> decrypted(encrypted);
    int decryptedSize = static_cast<int>(decrypted.size());
    CPPUNIT_ASSERT_EQUAL(0, ff_srtp_decrypt(&decryptContext, decrypted.data(), &decryptedSize));
    ff_srtp_free(&decryptContext);

    CPPUNIT_ASSERT_EQUAL(pli.size(), static_cast<size_t>(decryptedSize));
    CPPUNIT_ASSERT(std::memcmp(decrypted.data(), pli.data(), pli.size()) == 0);
}

void
RtcpCryptoTest::rtcpProtectionEncryptsOutgoingRtcp()
{
    UdpSocket remoteRtp;
    UdpSocket remoteRtcp;

    const dhtnet::IpAddr rtpDest {fmt::format("127.0.0.1:{}", remoteRtp.port())};
    const dhtnet::IpAddr rtcpDest {fmt::format("127.0.0.1:{}", remoteRtcp.port())};

    SocketPair socketPair(rtpDest, rtcpDest, reserveEphemeralPort(), reserveEphemeralPort());
    socketPair.createSRTP(SRTP_SUITE, SRTP_PARAMS, SRTP_SUITE, SRTP_PARAMS);
    socketPair.setRtcpProtection(true);

    const auto pli = SocketPair::createRtcpPli(0x11223344, 0x55667788);
    CPPUNIT_ASSERT(socketPair.writeRtcpData(pli.data(), static_cast<int>(pli.size())) > 0);

    const auto onWire = remoteRtcp.receive();
    CPPUNIT_ASSERT_EQUAL(pli.size() + SRTCP_OVERHEAD, onWire.size());
    CPPUNIT_ASSERT(std::memcmp(onWire.data() + 8, pli.data() + 8, 4) != 0);  // Payload encrypted
    CPPUNIT_ASSERT_EQUAL(uint8_t(0x80), uint8_t(onWire[pli.size()] & 0x80)); // E-bit

    SRTPContext decryptContext {};
    CPPUNIT_ASSERT_EQUAL(0, ff_srtp_set_crypto(&decryptContext, SRTP_SUITE, SRTP_PARAMS));
    std::vector<uint8_t> decrypted(onWire);
    int decryptedSize = static_cast<int>(decrypted.size());
    CPPUNIT_ASSERT_EQUAL(0, ff_srtp_decrypt(&decryptContext, decrypted.data(), &decryptedSize));
    ff_srtp_free(&decryptContext);
    CPPUNIT_ASSERT_EQUAL(pli.size(), static_cast<size_t>(decryptedSize));
    CPPUNIT_ASSERT(std::memcmp(decrypted.data(), pli.data(), pli.size()) == 0);
}

void
RtcpCryptoTest::legacyPeersStillReceivePlaintextRtcp()
{
    UdpSocket remoteRtp;
    UdpSocket remoteRtcp;

    const dhtnet::IpAddr rtpDest {fmt::format("127.0.0.1:{}", remoteRtp.port())};
    const dhtnet::IpAddr rtcpDest {fmt::format("127.0.0.1:{}", remoteRtcp.port())};

    // SDES-SRTP peers (legacy Jami) expect plaintext RTCP: protection must
    // stay opt-in so backward compatibility is preserved.
    SocketPair socketPair(rtpDest, rtcpDest, reserveEphemeralPort(), reserveEphemeralPort());
    socketPair.createSRTP(SRTP_SUITE, SRTP_PARAMS, SRTP_SUITE, SRTP_PARAMS);

    const auto pli = SocketPair::createRtcpPli(0x11223344, 0x55667788);
    CPPUNIT_ASSERT(socketPair.writeRtcpData(pli.data(), static_cast<int>(pli.size())) > 0);

    const auto onWire = remoteRtcp.receive();
    CPPUNIT_ASSERT_EQUAL(pli.size(), onWire.size());
    CPPUNIT_ASSERT(std::memcmp(onWire.data(), pli.data(), pli.size()) == 0);
}

void
RtcpCryptoTest::encryptedIncomingPliTriggersKeyframeCallback()
{
    UdpSocket remote;

    const auto localRtpPort = reserveEphemeralPort();
    const auto localRtcpPort = reserveEphemeralPort();
    const dhtnet::IpAddr rtpDest {fmt::format("127.0.0.1:{}", remote.port())};
    const dhtnet::IpAddr rtcpDest {fmt::format("127.0.0.1:{}", remote.port())};

    SocketPair socketPair(rtpDest, rtcpDest, localRtpPort, localRtcpPort);
    socketPair.createSRTP(SRTP_SUITE, SRTP_PARAMS, SRTP_SUITE, SRTP_PARAMS);
    socketPair.setRtcpProtection(true);
    socketPair.setReadBlockingMode(true);

    std::atomic_bool keyframeRequested {false};
    socketPair.setKeyframeRequestCallback([&keyframeRequested]() { keyframeRequested = true; });

    const auto pli = SocketPair::createRtcpPli(0x11223344, 0x55667788);
    remote.sendTo(localRtcpPort, protectRtcp(pli));

    std::unique_ptr<MediaIOHandle> ioHandle {socketPair.createIOContext(1500)};
    std::vector<uint8_t> buffer(2048);
    const auto readSize = avio_read_partial(ioHandle->getContext(), buffer.data(), static_cast<int>(buffer.size()));

    CPPUNIT_ASSERT(readSize > 0);
    CPPUNIT_ASSERT(keyframeRequested.load());

    // The decrypted packet must be exposed to the RTCP parsing pipeline.
    CPPUNIT_ASSERT_EQUAL(pli.size(), static_cast<size_t>(readSize));
    CPPUNIT_ASSERT(std::memcmp(buffer.data(), pli.data(), pli.size()) == 0);
}

void
RtcpCryptoTest::plaintextIncomingPliStillTriggersKeyframeCallback()
{
    UdpSocket remote;

    const auto localRtpPort = reserveEphemeralPort();
    const auto localRtcpPort = reserveEphemeralPort();
    const dhtnet::IpAddr rtpDest {fmt::format("127.0.0.1:{}", remote.port())};
    const dhtnet::IpAddr rtcpDest {fmt::format("127.0.0.1:{}", remote.port())};

    // Even with protection enabled, a plaintext PLI from a legacy peer must
    // still be understood.
    SocketPair socketPair(rtpDest, rtcpDest, localRtpPort, localRtcpPort);
    socketPair.createSRTP(SRTP_SUITE, SRTP_PARAMS, SRTP_SUITE, SRTP_PARAMS);
    socketPair.setRtcpProtection(true);
    socketPair.setReadBlockingMode(true);

    std::atomic_bool keyframeRequested {false};
    socketPair.setKeyframeRequestCallback([&keyframeRequested]() { keyframeRequested = true; });

    const auto pli = SocketPair::createRtcpPli(0x11223344, 0x55667788);
    remote.sendTo(localRtcpPort, pli);

    std::unique_ptr<MediaIOHandle> ioHandle {socketPair.createIOContext(1500)};
    std::vector<uint8_t> buffer(2048);
    const auto readSize = avio_read_partial(ioHandle->getContext(), buffer.data(), static_cast<int>(buffer.size()));

    CPPUNIT_ASSERT(readSize > 0);
    CPPUNIT_ASSERT(keyframeRequested.load());
}

void
RtcpCryptoTest::corruptedSrtcpDoesNotEndLiveStream()
{
    UdpSocket remote;

    const auto localRtpPort = reserveEphemeralPort();
    const auto localRtcpPort = reserveEphemeralPort();
    const dhtnet::IpAddr destination {fmt::format("127.0.0.1:{}", remote.port())};

    SocketPair socketPair(destination, destination, localRtpPort, localRtcpPort);
    socketPair.createSRTP(SRTP_SUITE, SRTP_PARAMS, SRTP_SUITE, SRTP_PARAMS);
    socketPair.setRtcpProtection(true);
    socketPair.setReadBlockingMode(true);

    const auto pli = SocketPair::createRtcpPli(0x11223344, 0x55667788);
    auto corrupted = protectRtcp(pli);
    corrupted.back() ^= 0xff;
    remote.sendTo(localRtcpPort, corrupted);

    std::unique_ptr<MediaIOHandle> ioHandle {socketPair.createIOContext(1500)};
    std::vector<uint8_t> buffer(2048);
    CPPUNIT_ASSERT_EQUAL(AVERROR(EAGAIN),
                         avio_read_partial(ioHandle->getContext(),
                                           buffer.data(),
                                           static_cast<int>(buffer.size())));

    remote.sendTo(localRtcpPort, protectRtcp(pli));
    const auto readSize = avio_read_partial(ioHandle->getContext(),
                                           buffer.data(),
                                           static_cast<int>(buffer.size()));
    CPPUNIT_ASSERT_EQUAL(pli.size(), static_cast<size_t>(readSize));
    CPPUNIT_ASSERT(std::memcmp(buffer.data(), pli.data(), pli.size()) == 0);
}

void
RtcpCryptoTest::rtcpByeDoesNotEndLiveStream()
{
    UdpSocket remoteRtp;
    UdpSocket remoteRtcp;

    const auto localRtpPort = reserveEphemeralPort();
    const auto localRtcpPort = reserveEphemeralPort();
    const dhtnet::IpAddr rtpDest {fmt::format("127.0.0.1:{}", remoteRtp.port())};
    const dhtnet::IpAddr rtcpDest {fmt::format("127.0.0.1:{}", remoteRtcp.port())};

    SocketPair socketPair(rtpDest, rtcpDest, localRtpPort, localRtcpPort);
    socketPair.setReadBlockingMode(true);

    const std::vector<uint8_t> bye {
        0x81, 203, 0x00, 0x01,
        0x11, 0x22, 0x33, 0x44,
    };
    remoteRtcp.sendTo(localRtcpPort, bye);

    std::unique_ptr<MediaIOHandle> ioHandle {socketPair.createIOContext(1500)};
    std::vector<uint8_t> buffer(2048);
    CPPUNIT_ASSERT_EQUAL(AVERROR(EAGAIN),
                         avio_read_partial(ioHandle->getContext(),
                                           buffer.data(),
                                           static_cast<int>(buffer.size())));

    const auto packet = rtpPacket(42);
    remoteRtp.sendTo(localRtpPort, packet);
    CPPUNIT_ASSERT_EQUAL(static_cast<int>(packet.size()),
                         avio_read_partial(ioHandle->getContext(),
                                           buffer.data(),
                                           static_cast<int>(buffer.size())));
}

void
RtcpCryptoTest::replayedRtpDoesNotEndStream()
{
    UdpSocket remote;

    const auto localRtpPort = reserveEphemeralPort();
    const auto localRtcpPort = reserveEphemeralPort();
    const dhtnet::IpAddr rtpDest {fmt::format("127.0.0.1:{}", remote.port())};
    const dhtnet::IpAddr rtcpDest {fmt::format("127.0.0.1:{}", remote.port())};

    SocketPair socketPair(rtpDest, rtcpDest, localRtpPort, localRtcpPort);
    socketPair.createSRTP(SRTP_SUITE, SRTP_PARAMS, SRTP_SUITE, SRTP_PARAMS);
    socketPair.setReadBlockingMode(true);

    const auto packet = rtpPacket(42);
    const auto encrypted = protectRtp(packet);
    std::unique_ptr<MediaIOHandle> ioHandle {socketPair.createIOContext(1500)};
    std::vector<uint8_t> buffer(2048);

    remote.sendTo(localRtpPort, encrypted);
    CPPUNIT_ASSERT_EQUAL(static_cast<int>(packet.size()),
                         avio_read_partial(ioHandle->getContext(),
                                           buffer.data(),
                                           static_cast<int>(buffer.size())));

    remote.sendTo(localRtpPort, encrypted);
    CPPUNIT_ASSERT_EQUAL(AVERROR(EAGAIN),
                         avio_read_partial(ioHandle->getContext(),
                                           buffer.data(),
                                           static_cast<int>(buffer.size())));

    const auto nextPacket = rtpPacket(43);
    remote.sendTo(localRtpPort, protectRtp(nextPacket));
    CPPUNIT_ASSERT_EQUAL(static_cast<int>(nextPacket.size()),
                         avio_read_partial(ioHandle->getContext(),
                                           buffer.data(),
                                           static_cast<int>(buffer.size())));
}

void
RtcpCryptoTest::stoppedReadIsCancelled()
{
    UdpSocket remote;

    const auto localRtpPort = reserveEphemeralPort();
    const auto localRtcpPort = reserveEphemeralPort();
    const dhtnet::IpAddr rtpDest {fmt::format("127.0.0.1:{}", remote.port())};
    const dhtnet::IpAddr rtcpDest {fmt::format("127.0.0.1:{}", remote.port())};

    SocketPair socketPair(rtpDest, rtcpDest, localRtpPort, localRtcpPort);
    std::unique_ptr<MediaIOHandle> ioHandle {socketPair.createIOContext(1500)};
    std::vector<uint8_t> buffer(2048);
    socketPair.setReadBlockingMode(false);
    CPPUNIT_ASSERT_EQUAL(AVERROR_EXIT,
                         avio_read_partial(ioHandle->getContext(),
                                           buffer.data(),
                                           static_cast<int>(buffer.size())));

    socketPair.setReadBlockingMode(true);
    socketPair.interrupt();
    CPPUNIT_ASSERT_EQUAL(AVERROR_EXIT,
                         avio_read_partial(ioHandle->getContext(),
                                           buffer.data(),
                                           static_cast<int>(buffer.size())));
}

} // namespace test
} // namespace jami

JAMI_TEST_RUNNER(jami::test::RtcpCryptoTest::name());
