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
#include <fmt/format.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <git2.h>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

#include "fileutils.h"
#include "jamidht/gitserver.h"

#include "../../test_runner.h"

namespace jami {
namespace test {

using namespace std::literals::chrono_literals;

/**
 * A ChannelSocketInterface that records what GitServer does to it.
 *
 * It deliberately mimics the parts of dhtnet::ChannelSocket that GitServer
 * relies on: shutdown() is idempotent and notifies the shutdown callback,
 * and setOnRecv() may be called any number of times, including with an empty
 * callback to clear it.
 */
class FakeChannelSocket : public dhtnet::ChannelSocketInterface
{
public:
    // dhtnet::GenericSocket
    void shutdown() override
    {
        if (isShutdown_.exchange(true))
            return;
        std::lock_guard lk(mutex_);
        if (shutdownCb_)
            shutdownCb_({});
        cv_.notify_all();
    }
    void setOnRecv(RecvCb&& cb) override
    {
        std::lock_guard lk(mutex_);
        recvCb_ = std::move(cb);
    }
    bool isReliable() const override { return true; }
    bool isInitiator() const override { return false; }
    int maxPayload() const override { return 0; }
    int waitForData(std::chrono::milliseconds, std::error_code&) const override { return 0; }
    std::size_t write(const ValueType* buf, std::size_t len, std::error_code& ec) override
    {
        ec = {};
        // Not mutex_: the server writes its answers from within the receive callback.
        std::lock_guard lk(writeMutex_);
        written_.insert(written_.end(), buf, buf + len);
        return len;
    }
    std::size_t read(ValueType*, std::size_t, std::error_code& ec) override
    {
        ec = {};
        return 0;
    }

    // dhtnet::ChannelSocketInterface
    dhtnet::DeviceId deviceId() const override { return {}; }
    const std::string& name() const override { return name_; }
    uint16_t channel() const override { return 42; }
    void onReady(dhtnet::ChannelReadyCb&&) override {}
    void onShutdown(dhtnet::OnShutdownCb&& cb) override
    {
        std::unique_lock lk(mutex_);
        shutdownCb_ = std::move(cb);
        if (isShutdown_) {
            lk.unlock();
            shutdownCb_({});
        }
    }
    void onRecv(std::vector<uint8_t>&& pkt) override
    {
        std::lock_guard lk(mutex_);
        if (recvCb_)
            recvCb_(pkt.data(), pkt.size());
    }
    uint64_t txBytes() const override { return 0; }
    uint64_t rxBytes() const override { return 0; }
    std::chrono::steady_clock::time_point getStartTime() const override { return start_; }

    bool isShutdown() const { return isShutdown_; }

    /** Block until the socket is shut down, or the timeout expires. */
    bool waitForShutdown(std::chrono::milliseconds timeout)
    {
        std::unique_lock lk(mutex_);
        return cv_.wait_for(lk, timeout, [this] { return isShutdown_.load(); });
    }

    bool hasRecvCb() const
    {
        std::lock_guard lk(mutex_);
        return static_cast<bool>(recvCb_);
    }

    std::vector<uint8_t> written() const
    {
        std::lock_guard lk(writeMutex_);
        return written_;
    }

private:
    mutable std::mutex mutex_ {};
    mutable std::mutex writeMutex_ {};
    std::condition_variable cv_ {};
    std::atomic_bool isShutdown_ {false};
    RecvCb recvCb_ {};
    dhtnet::OnShutdownCb shutdownCb_ {};
    std::vector<uint8_t> written_ {};
    std::string name_ {"git://fake/conversation"};
    std::chrono::steady_clock::time_point start_ {std::chrono::steady_clock::now()};
};

class GitServerTest : public CppUnit::TestFixture
{
public:
    ~GitServerTest() = default;
    static std::string name() { return "gitserver"; }

    void setUp() override;
    void tearDown() override;

private:
    void testStopShutsDownChannel();
    void testDestructorShutsDownChannel();
    void testStopIsIdempotent();
    void testPackOnlyHasMissingObjects();
    void testPackIgnoresUnknownHaves();

    CPPUNIT_TEST_SUITE(GitServerTest);
    CPPUNIT_TEST(testStopShutsDownChannel);
    CPPUNIT_TEST(testDestructorShutsDownChannel);
    CPPUNIT_TEST(testStopIsIdempotent);
    CPPUNIT_TEST(testPackOnlyHasMissingObjects);
    CPPUNIT_TEST(testPackIgnoresUnknownHaves);
    CPPUNIT_TEST_SUITE_END();

    std::pair<std::string, std::string> writeHistory();
    uint32_t fetchPackObjectCount(const std::string& want, const std::string& have);

    // GitServer derives the repository path from the account and conversation
    // ids, so the fixture has to place a real repository where it will look.
    std::string accountId_ {"gitservertest"};
    std::string conversationId_ {"0123456789abcdef0123456789abcdef01234567"};
    std::filesystem::path repoPath_ {};
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(GitServerTest, GitServerTest::name());

void
GitServerTest::setUp()
{
    git_libgit2_init();
    repoPath_ = fileutils::get_data_dir() / accountId_ / "conversations" / conversationId_;
    std::filesystem::remove_all(repoPath_);
    std::filesystem::create_directories(repoPath_);

    git_repository* repo = nullptr;
    CPPUNIT_ASSERT_EQUAL(0, git_repository_init(&repo, repoPath_.string().c_str(), false));
    git_repository_free(repo);
}

void
GitServerTest::tearDown()
{
    std::error_code ec;
    std::filesystem::remove_all(fileutils::get_data_dir() / accountId_, ec);
    git_libgit2_shutdown();
}

void
GitServerTest::testStopShutsDownChannel()
{
    auto socket = std::make_shared<FakeChannelSocket>();
    auto gitServer = std::make_unique<GitServer>(accountId_, conversationId_, socket);

    CPPUNIT_ASSERT(!socket->isShutdown());

    gitServer->stop();

    // stop() must take effect on the first call. The actual shutdown is
    // dispatched to the IO pool, hence the wait.
    CPPUNIT_ASSERT(socket->waitForShutdown(10s));
    // And no further data may be dispatched into the (now stopped) server.
    CPPUNIT_ASSERT(!socket->hasRecvCb());
}

void
GitServerTest::testDestructorShutsDownChannel()
{
    auto socket = std::make_shared<FakeChannelSocket>();
    { auto gitServer = std::make_unique<GitServer>(accountId_, conversationId_, socket); }

    CPPUNIT_ASSERT(socket->waitForShutdown(10s));
    CPPUNIT_ASSERT(!socket->hasRecvCb());
}

void
GitServerTest::testStopIsIdempotent()
{
    auto socket = std::make_shared<FakeChannelSocket>();
    auto gitServer = std::make_unique<GitServer>(accountId_, conversationId_, socket);

    gitServer->stop();
    gitServer->stop();
    gitServer.reset();

    CPPUNIT_ASSERT(socket->waitForShutdown(10s));
}

namespace {

git_oid
writeBlob(git_repository* repo, std::string_view content)
{
    git_oid id;
    CPPUNIT_ASSERT_EQUAL(0, git_blob_create_from_buffer(&id, repo, content.data(), content.size()));
    return id;
}

git_oid
writeTree(git_repository* repo, const std::vector<std::tuple<std::string, git_oid, git_filemode_t>>& entries)
{
    git_treebuilder* builder = nullptr;
    CPPUNIT_ASSERT_EQUAL(0, git_treebuilder_new(&builder, repo, nullptr));
    for (const auto& [name, id, mode] : entries)
        CPPUNIT_ASSERT_EQUAL(0, git_treebuilder_insert(nullptr, builder, name.c_str(), &id, mode));
    git_oid id;
    CPPUNIT_ASSERT_EQUAL(0, git_treebuilder_write(&id, builder));
    git_treebuilder_free(builder);
    return id;
}

git_oid
writeCommit(git_repository* repo, const git_oid& treeId, const git_oid* parentId)
{
    git_signature* sig = nullptr;
    CPPUNIT_ASSERT_EQUAL(0, git_signature_new(&sig, "test", "test@jami.net", 1700000000, 0));
    git_tree* tree = nullptr;
    CPPUNIT_ASSERT_EQUAL(0, git_tree_lookup(&tree, repo, &treeId));
    git_commit* parent = nullptr;
    if (parentId)
        CPPUNIT_ASSERT_EQUAL(0, git_commit_lookup(&parent, repo, parentId));
    git_oid id;
    CPPUNIT_ASSERT_EQUAL(
        0, git_commit_create_v(&id, repo, nullptr, sig, sig, nullptr, "commit", tree, parent ? 1 : 0, parent));
    git_commit_free(parent);
    git_tree_free(tree);
    git_signature_free(sig);
    return id;
}

std::vector<uint8_t>
pktLine(std::string_view payload)
{
    auto line = fmt::format("{:04x}{}", payload.size() + 4, payload);
    return {line.begin(), line.end()};
}

} // namespace

std::pair<std::string, std::string>
GitServerTest::writeHistory()
{
    // Shaped like a conversation: the certificates of its admins, devices and
    // members, then a second commit that adds a member.
    git_repository* repo = nullptr;
    CPPUNIT_ASSERT_EQUAL(0, git_repository_open(&repo, repoPath_.string().c_str()));
    auto member = writeBlob(repo, "member certificate");
    auto profile = writeBlob(repo, "profile");
    auto admins = writeTree(repo, {{"admin.crt", writeBlob(repo, "admin certificate"), GIT_FILEMODE_BLOB}});
    auto devices = writeTree(repo, {{"device.crt", writeBlob(repo, "device certificate"), GIT_FILEMODE_BLOB}});
    auto members = writeTree(repo, {{"member.crt", member, GIT_FILEMODE_BLOB}});
    auto moreMembers = writeTree(repo,
                                 {{"member.crt", member, GIT_FILEMODE_BLOB},
                                  {"new.crt", writeBlob(repo, "new member certificate"), GIT_FILEMODE_BLOB}});
    auto first = writeCommit(repo,
                             writeTree(repo,
                                       {{"admins", admins, GIT_FILEMODE_TREE},
                                        {"devices", devices, GIT_FILEMODE_TREE},
                                        {"members", members, GIT_FILEMODE_TREE},
                                        {"profile.vcf", profile, GIT_FILEMODE_BLOB}}),
                             nullptr);
    auto second = writeCommit(repo,
                              writeTree(repo,
                                        {{"admins", admins, GIT_FILEMODE_TREE},
                                         {"devices", devices, GIT_FILEMODE_TREE},
                                         {"members", moreMembers, GIT_FILEMODE_TREE},
                                         {"profile.vcf", profile, GIT_FILEMODE_BLOB}}),
                              &first);
    git_repository_free(repo);
    std::string firstId = git_oid_tostr_s(&first);
    std::string secondId = git_oid_tostr_s(&second);
    return {firstId, secondId};
}

uint32_t
GitServerTest::fetchPackObjectCount(const std::string& want, const std::string& have)
{
    auto socket = std::make_shared<FakeChannelSocket>();
    GitServer gitServer(accountId_, conversationId_, socket);
    socket->onRecv(pktLine(fmt::format("want {}\n", want)));
    socket->onRecv(pktLine(fmt::format("have {}\n", have)));
    socket->onRecv({'0', '0', '0', '0'});
    socket->onRecv(pktLine("done\n"));

    // The pack is what the server sent on side-band channel 1
    auto out = socket->written();
    std::vector<uint8_t> pack;
    for (std::size_t pos = 0; pos + 4 <= out.size();) {
        unsigned len = 0;
        std::from_chars(reinterpret_cast<const char*>(out.data() + pos),
                        reinterpret_cast<const char*>(out.data() + pos + 4),
                        len,
                        16);
        if (len == 0) {
            pos += 4;
            continue;
        }
        CPPUNIT_ASSERT(len > 4 && pos + len <= out.size());
        if (out[pos + 4] == 1)
            pack.insert(pack.end(), out.begin() + pos + 5, out.begin() + pos + len);
        pos += len;
    }
    // "PACK", the version, then the number of objects, in network order
    CPPUNIT_ASSERT(pack.size() >= 12 && std::equal(pack.begin(), pack.begin() + 4, "PACK"));
    return (uint32_t(pack[8]) << 24) | (uint32_t(pack[9]) << 16) | (uint32_t(pack[10]) << 8) | pack[11];
}

void
GitServerTest::testPackOnlyHasMissingObjects()
{
    auto [first, second] = writeHistory();
    // The second commit, its root tree, the members tree and the new certificate.
    // Neither the unchanged trees nor the certificates the peer has.
    CPPUNIT_ASSERT_EQUAL(uint32_t(4), fetchPackObjectCount(second, first));
}

void
GitServerTest::testPackIgnoresUnknownHaves()
{
    auto [first, second] = writeHistory();
    // A commit we lack hides nothing: both commits, 6 trees and 5 blobs.
    CPPUNIT_ASSERT_EQUAL(uint32_t(13), fetchPackObjectCount(second, std::string(40, 'a')));
}

} // namespace test
} // namespace jami

JAMI_TEST_RUNNER(jami::test::GitServerTest::name())
