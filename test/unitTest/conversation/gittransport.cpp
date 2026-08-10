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

#include <dhtnet/channel_socket.h>

#include <atomic>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "gittransport.h"

#include "../../test_runner.h"

namespace jami {
namespace test {

/**
 * A ChannelSocketInterface that feeds P2PStream canned data and can be told to
 * fail its writes, so the transport can be exercised without a peer.
 */
class FakeChannelSocket : public dhtnet::ChannelSocketInterface
{
public:
    // dhtnet::GenericSocket
    void shutdown() override { isShutdown_ = true; }
    void setOnRecv(RecvCb&&) override {}
    bool isReliable() const override { return true; }
    bool isInitiator() const override { return false; }
    int maxPayload() const override { return 0; }
    int waitForData(std::chrono::milliseconds timeout, std::error_code& ec) const override
    {
        std::lock_guard lk(mutex_);
        waited_ = timeout;
        // Same contract as ChannelSocket: untouched on a timeout, set once the
        // channel is closed and drained.
        if (pending_.empty() and isShutdown_)
            ec = std::make_error_code(std::errc::broken_pipe);
        else
            ec = {};
        return pending_.size();
    }
    std::size_t write(const ValueType* buf, std::size_t len, std::error_code& ec) override
    {
        std::lock_guard lk(mutex_);
        if (writeError_) {
            ec = writeError_;
            // ChannelSocket reports the bytes it managed to push before failing,
            // which for a multiplexed endpoint error is a positive count.
            return len / 2;
        }
        ec = {};
        written_.insert(written_.end(), buf, buf + len);
        return len;
    }
    std::size_t read(ValueType* buf, std::size_t len, std::error_code& ec) override
    {
        ec = {};
        std::lock_guard lk(mutex_);
        auto size = std::min(len, pending_.size());
        std::memcpy(buf, pending_.data(), size);
        pending_.erase(pending_.begin(), pending_.begin() + size);
        return size;
    }

    // dhtnet::ChannelSocketInterface
    dhtnet::DeviceId deviceId() const override { return {}; }
    const std::string& name() const override { return name_; }
    uint16_t channel() const override { return 42; }
    void onReady(dhtnet::ChannelReadyCb&&) override {}
    void onShutdown(dhtnet::OnShutdownCb&&) override {}
    void onRecv(std::vector<uint8_t>&&) override {}
    uint64_t txBytes() const override { return 0; }
    uint64_t rxBytes() const override { return 0; }
    std::chrono::steady_clock::time_point getStartTime() const override { return start_; }

    void failWrites(std::error_code ec)
    {
        std::lock_guard lk(mutex_);
        writeError_ = ec;
    }

    void queue(std::string_view data)
    {
        std::lock_guard lk(mutex_);
        pending_.insert(pending_.end(), data.begin(), data.end());
    }

    std::vector<uint8_t> written() const
    {
        std::lock_guard lk(mutex_);
        return written_;
    }

    std::chrono::milliseconds waited() const
    {
        std::lock_guard lk(mutex_);
        return waited_;
    }

private:
    mutable std::mutex mutex_ {};
    mutable std::chrono::milliseconds waited_ {};
    std::atomic_bool isShutdown_ {false};
    std::error_code writeError_ {};
    std::vector<uint8_t> pending_ {};
    std::vector<uint8_t> written_ {};
    std::string name_ {"git://fake/conversation"};
    std::chrono::steady_clock::time_point start_ {std::chrono::steady_clock::now()};
};

class GitTransportTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "gittransport"; }

    void setUp() override;
    void tearDown() override;

private:
    void testSendCmdWritesTheRequest();
    void testSendCmdReportsAWriteError();
    void testReadReturnsZeroOnSuccess();
    void testReadFailsWhenTheChannelIsClosed();
    void testReadReturnsZeroOnAnIdleChannel();
    void testReadGivesUpOnAQuietPeer();
    void testReadFailsWhenTheCommandCannotBeSent();

    CPPUNIT_TEST_SUITE(GitTransportTest);
    CPPUNIT_TEST(testSendCmdWritesTheRequest);
    CPPUNIT_TEST(testSendCmdReportsAWriteError);
    CPPUNIT_TEST(testReadReturnsZeroOnSuccess);
    CPPUNIT_TEST(testReadFailsWhenTheChannelIsClosed);
    CPPUNIT_TEST(testReadReturnsZeroOnAnIdleChannel);
    CPPUNIT_TEST(testReadGivesUpOnAQuietPeer);
    CPPUNIT_TEST(testReadFailsWhenTheCommandCannotBeSent);
    CPPUNIT_TEST_SUITE_END();

    std::shared_ptr<FakeChannelSocket> socket_;
    P2PStream stream_;
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(GitTransportTest, GitTransportTest::name());

void
GitTransportTest::setUp()
{
    // git_buf_set() allocates through libgit2's allocator, so the library has
    // to be up before a request can be generated.
    git_libgit2_init();
    socket_ = std::make_shared<FakeChannelSocket>();
    stream_ = {};
    stream_.socket = socket_;
    stream_.base.read = P2PStreamRead;
    stream_.base.write = P2PStreamWrite;
    stream_.base.free = P2PStreamFree;
    stream_.cmd = UPLOAD_PACK_CMD;
    stream_.url = "deviceId/conversationId";
    stream_.sent_command = 0;
}

void
GitTransportTest::tearDown()
{
    socket_.reset();
    git_libgit2_shutdown();
}

void
GitTransportTest::testSendCmdWritesTheRequest()
{
    CPPUNIT_ASSERT_EQUAL(0, sendCmd(&stream_));
    CPPUNIT_ASSERT(stream_.sent_command);

    auto written = socket_->written();
    std::string request(written.begin(), written.end());
    CPPUNIT_ASSERT(request.find(UPLOAD_PACK_CMD) != std::string::npos);
    CPPUNIT_ASSERT(request.find("conversationId") != std::string::npos);
}

void
GitTransportTest::testSendCmdReportsAWriteError()
{
    socket_->failWrites(std::make_error_code(std::errc::broken_pipe));

    CPPUNIT_ASSERT_EQUAL(-1, sendCmd(&stream_));
    // The peer never got the whole command, so it must not be considered sent.
    CPPUNIT_ASSERT(!stream_.sent_command);
}

void
GitTransportTest::testReadReturnsZeroOnSuccess()
{
    socket_->queue("0008NAK\n");

    char buffer[64];
    size_t read = 42;
    CPPUNIT_ASSERT_EQUAL(0, P2PStreamRead(&stream_.base, buffer, sizeof(buffer), &read));
    CPPUNIT_ASSERT_EQUAL(size_t(8), read);
    CPPUNIT_ASSERT_EQUAL(std::string("0008NAK\n"), std::string(buffer, read));
}

void
GitTransportTest::testReadFailsWhenTheChannelIsClosed()
{
    socket_->shutdown();

    char buffer[64];
    size_t read = 42;
    // The peer ends a packfile with a flush packet, so a closed channel is
    // always a dead fetch and never a normal end of stream.
    CPPUNIT_ASSERT(P2PStreamRead(&stream_.base, buffer, sizeof(buffer), &read) < 0);
    CPPUNIT_ASSERT_EQUAL(size_t(0), read);
}

void
GitTransportTest::testReadReturnsZeroOnAnIdleChannel()
{
    char buffer[64];
    size_t read = 42;
    CPPUNIT_ASSERT_EQUAL(0, P2PStreamRead(&stream_.base, buffer, sizeof(buffer), &read));
    CPPUNIT_ASSERT_EQUAL(size_t(0), read);
}

void
GitTransportTest::testReadGivesUpOnAQuietPeer()
{
    char buffer[64];
    size_t read = 42;
    P2PStreamRead(&stream_.base, buffer, sizeof(buffer), &read);

    // The wait holds ConversationRepository::opMtx_, which every commit on the
    // conversation needs. A peer that stops sending must cost a retry, not
    // every message the user writes until it comes back.
    CPPUNIT_ASSERT(socket_->waited() > std::chrono::seconds(0));
    CPPUNIT_ASSERT(socket_->waited() <= std::chrono::minutes(5));
}

void
GitTransportTest::testReadFailsWhenTheCommandCannotBeSent()
{
    socket_->failWrites(std::make_error_code(std::errc::broken_pipe));
    socket_->queue("0008NAK\n");

    char buffer[64];
    size_t read = 42;
    CPPUNIT_ASSERT(P2PStreamRead(&stream_.base, buffer, sizeof(buffer), &read) < 0);
    CPPUNIT_ASSERT_EQUAL(size_t(0), read);
}

} // namespace test
} // namespace jami

JAMI_TEST_RUNNER(jami::test::GitTransportTest::name())
