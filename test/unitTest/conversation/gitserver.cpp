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
#include <random>
#include <string>
#include <tuple>
#include <vector>

#include "fileutils.h"
#include "jamidht/gitserver.h"
#include "jamidht/circle_fetch.h"
#include "jamidht/circle_module.h"

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
        if (writeObserver_)
            writeObserver_(buf, len);
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

    void observeWrites(std::function<void(const uint8_t*, size_t)> observer)
    {
        std::lock_guard lock(writeMutex_);
        writeObserver_ = std::move(observer);
    }

private:
    mutable std::mutex mutex_ {};
    mutable std::mutex writeMutex_ {};
    std::condition_variable cv_ {};
    std::atomic_bool isShutdown_ {false};
    RecvCb recvCb_ {};
    dhtnet::OnShutdownCb shutdownCb_ {};
    std::vector<uint8_t> written_ {};
    std::function<void(const uint8_t*, size_t)> writeObserver_;
    std::string name_ {"git://fake/conversation"};
    std::chrono::steady_clock::time_point start_ {std::chrono::steady_clock::now()};
};

/**
 * Client end of an in-memory channel: writes feed the server socket, and the
 * server's answers are buffered for blocking reads, like a dhtnet channel.
 */
class PipeSocket : public dhtnet::ChannelSocketInterface
{
public:
    explicit PipeSocket(std::shared_ptr<FakeChannelSocket> server)
        : server_(std::move(server))
    {
        server_->observeWrites([this](const uint8_t* data, size_t size) {
            std::lock_guard lock(mutex_);
            buffer_.insert(buffer_.end(), data, data + size);
            cv_.notify_all();
        });
    }
    ~PipeSocket() { server_->observeWrites({}); }

    void shutdown() override
    {
        std::lock_guard lock(mutex_);
        closed_ = true;
        cv_.notify_all();
    }
    void setOnRecv(RecvCb&&) override {}
    bool isReliable() const override { return true; }
    bool isInitiator() const override { return true; }
    int maxPayload() const override { return 0; }
    int waitForData(std::chrono::milliseconds timeout, std::error_code& ec) const override
    {
        std::unique_lock lock(mutex_);
        cv_.wait_for(lock, timeout, [this] { return !buffer_.empty() || closed_; });
        if (buffer_.empty())
            ec = std::make_error_code(std::errc::connection_aborted);
        return static_cast<int>(buffer_.size());
    }
    std::size_t write(const ValueType* buf, std::size_t len, std::error_code& ec) override
    {
        ec = {};
        server_->onRecv(std::vector<uint8_t>(buf, buf + len));
        return len;
    }
    std::size_t read(ValueType* buf, std::size_t len, std::error_code& ec) override
    {
        ec = {};
        std::lock_guard lock(mutex_);
        const auto size = std::min(len, buffer_.size());
        std::copy_n(buffer_.begin(), size, buf);
        buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(size));
        return size;
    }

    dhtnet::DeviceId deviceId() const override { return {}; }
    const std::string& name() const override { return name_; }
    uint16_t channel() const override { return 43; }
    void onReady(dhtnet::ChannelReadyCb&&) override {}
    void onShutdown(dhtnet::OnShutdownCb&&) override {}
    void onRecv(std::vector<uint8_t>&&) override {}
    uint64_t txBytes() const override { return 0; }
    uint64_t rxBytes() const override { return 0; }
    std::chrono::steady_clock::time_point getStartTime() const override { return start_; }

private:
    std::shared_ptr<FakeChannelSocket> server_;
    mutable std::mutex mutex_;
    mutable std::condition_variable cv_;
    std::vector<uint8_t> buffer_;
    bool closed_ {false};
    std::string name_ {"git://fake/circles"};
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
    void testGuardedServerDeniesOutput();
    void testGuardedServerRevocation();
    void testGuardedServerRejectsUnreachableWant();
    void testFragmentedAdvertisementRequest();
    void testMountedCircleServing();
    void testGuardedPackTransfer();
    void testRevocationDuringPack();
    void testCircleFetchOverChannel();

    CPPUNIT_TEST_SUITE(GitServerTest);
    CPPUNIT_TEST(testStopShutsDownChannel);
    CPPUNIT_TEST(testDestructorShutsDownChannel);
    CPPUNIT_TEST(testStopIsIdempotent);
    CPPUNIT_TEST(testPackOnlyHasMissingObjects);
    CPPUNIT_TEST(testPackIgnoresUnknownHaves);
    CPPUNIT_TEST(testGuardedServerDeniesOutput);
    CPPUNIT_TEST(testGuardedServerRevocation);
    CPPUNIT_TEST(testGuardedServerRejectsUnreachableWant);
    CPPUNIT_TEST(testFragmentedAdvertisementRequest);
    CPPUNIT_TEST(testMountedCircleServing);
    CPPUNIT_TEST(testGuardedPackTransfer);
    CPPUNIT_TEST(testRevocationDuringPack);
    CPPUNIT_TEST(testCircleFetchOverChannel);
    CPPUNIT_TEST_SUITE_END();

    std::pair<std::string, std::string> writeHistory();
    void publishHead(const std::string& id);
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
    {
        auto gitServer = std::make_unique<GitServer>(accountId_, conversationId_, socket);
    }

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

std::function<std::optional<std::string>()>
granted(std::string head)
{
    return [head = std::move(head)]() -> std::optional<std::string> {
        return head;
    };
}

std::function<std::optional<std::string>()>
grantedWhile(const bool& allowed, std::string head)
{
    return [&allowed, head = std::move(head)]() -> std::optional<std::string> {
        return allowed ? std::optional<std::string>(head) : std::nullopt;
    };
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
GitServerTest::publishHead(const std::string& id)
{
    git_repository* raw = nullptr;
    CPPUNIT_ASSERT(git_repository_open(&raw, repoPath_.string().c_str()) == 0);
    GitRepository repository(raw);
    git_oid oid {};
    CPPUNIT_ASSERT(git_oid_fromstr(&oid, id.c_str()) == 0);
    git_reference* referenceRaw = nullptr;
    CPPUNIT_ASSERT(git_reference_create(&referenceRaw, repository.get(), "refs/heads/main", &oid, 1, "test") == 0);
    GitReference reference(referenceRaw);
    CPPUNIT_ASSERT(git_repository_set_head(repository.get(), "refs/heads/main") == 0);
}

void
GitServerTest::testGuardedServerDeniesOutput()
{
    const auto [first, second] = writeHistory();
    publishHead(second);
    const auto socket = std::make_shared<FakeChannelSocket>();
    GitServer server(accountId_, conversationId_, socket, repoPath_, []() -> std::optional<std::string> {
        return std::nullopt;
    });
    CPPUNIT_ASSERT(socket->waitForShutdown(10s));
    CPPUNIT_ASSERT(socket->written().empty());
}

void
GitServerTest::testGuardedServerRevocation()
{
    const auto [first, second] = writeHistory();
    publishHead(second);
    bool allowed = true;
    const auto socket = std::make_shared<FakeChannelSocket>();
    GitServer server(accountId_, conversationId_, socket, repoPath_, grantedWhile(allowed, second));
    socket->onRecv(pktLine("git-upload-pack /circles/v1/test"));
    const auto before = socket->written();
    CPPUNIT_ASSERT(!before.empty());
    allowed = false;
    socket->onRecv(pktLine(fmt::format("want {}\n", second)));
    socket->onRecv(pktLine("done\n"));
    CPPUNIT_ASSERT(socket->waitForShutdown(10s));
    CPPUNIT_ASSERT(socket->written() == before);
}

void
GitServerTest::testGuardedServerRejectsUnreachableWant()
{
    const auto [first, second] = writeHistory();
    publishHead(second);
    git_repository* raw = nullptr;
    CPPUNIT_ASSERT(git_repository_open(&raw, repoPath_.string().c_str()) == 0);
    GitRepository repository(raw);
    git_oid oid {};
    CPPUNIT_ASSERT(git_oid_fromstr(&oid, second.c_str()) == 0);
    git_reference* referenceRaw = nullptr;
    CPPUNIT_ASSERT(git_reference_create(&referenceRaw, repository.get(), "refs/heads/private", &oid, 1, "test") == 0);
    GitReference reference(referenceRaw);
    const auto socket = std::make_shared<FakeChannelSocket>();
    GitServer server(accountId_, conversationId_, socket, repoPath_, granted(first));
    socket->onRecv(pktLine("git-upload-pack /circles/v1/test"));
    const auto before = socket->written();
    const std::string advertised(before.begin(), before.end());
    CPPUNIT_ASSERT(advertised.find("refs/heads/private") == std::string::npos);
    CPPUNIT_ASSERT(advertised.find(first) != std::string::npos);
    CPPUNIT_ASSERT(advertised.find(second) == std::string::npos);
    socket->onRecv(pktLine(fmt::format("want {}\n", second)));
    socket->onRecv(pktLine("done\n"));
    CPPUNIT_ASSERT(socket->waitForShutdown(10s));
    CPPUNIT_ASSERT(socket->written() == before);
}

void
GitServerTest::testFragmentedAdvertisementRequest()
{
    const auto [first, second] = writeHistory();
    publishHead(second);
    const auto socket = std::make_shared<FakeChannelSocket>();
    GitServer server(accountId_, conversationId_, socket, repoPath_, granted(second));
    const auto request = pktLine("git-upload-pack /circles/v1/test");
    socket->onRecv({request.begin(), request.begin() + 2});
    CPPUNIT_ASSERT(socket->written().empty());
    socket->onRecv({request.begin() + 2, request.begin() + 6});
    CPPUNIT_ASSERT(socket->written().empty());
    socket->onRecv({request.begin() + 6, request.end()});
    const auto output = socket->written();
    CPPUNIT_ASSERT(!output.empty());
    CPPUNIT_ASSERT(std::string(output.begin(), output.end()).find(second) != std::string::npos);
}

void
GitServerTest::testMountedCircleServing()
{
    const auto account = dht::crypto::generateIdentity("serving-account", {}, 2048, true);
    const auto identity = dht::crypto::generateIdentity("serving-device", account, 2048, false);
    const auto peerAccount = dht::crypto::generateIdentity("invited-account", {}, 2048, true);
    const auto peer = dht::crypto::generateIdentity("invited-device", peerAccount, 2048, false);
    const auto path = repoPath_ / "circle.git";
    const auto circle = CircleRepository::create(path, identity);
    CPPUNIT_ASSERT(circle);
    const auto member = peerAccount.second->getId().toString();
    CPPUNIT_ASSERT(circle->append(identity, InviteCircleMember {member}));
    CircleModule runtime(identity, [](const auto&) { return true; });
    CPPUNIT_ASSERT(runtime.mountCircle(path, circle->id()));
    const auto socket = std::make_shared<FakeChannelSocket>();
    const auto session = runtime.open("circles/v1/" + circle->id(), peer.second, [socket] { socket->shutdown(); });
    CPPUNIT_ASSERT(session);
    GitServer server(accountId_, circle->id(), socket, session->repository, [session] { return session->grant(); });
    socket->onRecv(pktLine("git-upload-pack /circles/v1/" + circle->id()));
    const auto before = socket->written();
    CPPUNIT_ASSERT(!before.empty());
    const auto ban = circle->moderate(identity, {CircleModerationAction::BAN_MEMBER, member});
    CPPUNIT_ASSERT(ban && ban->resolution);
    CPPUNIT_ASSERT(runtime.refresh(circle->id()));
    CPPUNIT_ASSERT(socket->isShutdown());
    socket->onRecv(pktLine(fmt::format("want {}\n", circle->head())));
    CPPUNIT_ASSERT(socket->written() == before);
}

void
GitServerTest::testGuardedPackTransfer()
{
    const auto [first, second] = writeHistory();
    publishHead(second);
    const auto socket = std::make_shared<FakeChannelSocket>();
    GitServer server(accountId_, conversationId_, socket, repoPath_, granted(second));
    socket->onRecv(pktLine("git-upload-pack /circles/v1/test"));
    socket->onRecv(pktLine(fmt::format("want {}\n", second)));
    socket->onRecv(pktLine(fmt::format("have {}\n", first)));
    socket->onRecv({'0', '0', '0', '0'});
    socket->onRecv(pktLine("done\n"));
    const auto output = socket->written();
    const std::string_view magic = "PACK";
    const auto found = std::search(output.begin(), output.end(), magic.begin(), magic.end());
    CPPUNIT_ASSERT(found != output.end() && std::distance(found, output.end()) >= 12);
    const auto count = (uint32_t(found[8]) << 24) | (uint32_t(found[9]) << 16) | (uint32_t(found[10]) << 8) | found[11];
    CPPUNIT_ASSERT_EQUAL(uint32_t(4), count);
    CPPUNIT_ASSERT(!socket->isShutdown());
}

void
GitServerTest::testRevocationDuringPack()
{
    const auto [first, second] = writeHistory();
    git_repository* raw = nullptr;
    CPPUNIT_ASSERT(git_repository_open(&raw, repoPath_.string().c_str()) == 0);
    GitRepository repository(raw);
    std::string large(200000, 'x');
    std::mt19937 random(17);
    for (auto& character : large)
        character = static_cast<char>(random() & 0xff);
    git_oid parent {};
    CPPUNIT_ASSERT(git_oid_fromstr(&parent, second.c_str()) == 0);
    const auto latest = writeCommit(repository.get(),
                                    writeTree(repository.get(),
                                              {{"large", writeBlob(repository.get(), large), GIT_FILEMODE_BLOB}}),
                                    &parent);
    const std::string head = git_oid_tostr_s(&latest);
    publishHead(head);
    bool allowed = true;
    unsigned chunks = 0;
    const auto socket = std::make_shared<FakeChannelSocket>();
    socket->observeWrites([&](const uint8_t* data, size_t size) {
        if (size > 5 && data[4] == 1) {
            ++chunks;
            allowed = false;
        }
    });
    GitServer server(accountId_, conversationId_, socket, repoPath_, grantedWhile(allowed, head));
    socket->onRecv(pktLine("git-upload-pack /circles/v1/test"));
    socket->onRecv(pktLine(fmt::format("want {}\n", head)));
    socket->onRecv(pktLine("done\n"));
    CPPUNIT_ASSERT(socket->waitForShutdown(10s));
    CPPUNIT_ASSERT(chunks == 1);
    CPPUNIT_ASSERT(socket->written().size() < large.size());
}

void
GitServerTest::testCircleFetchOverChannel()
{
    const auto account = dht::crypto::generateIdentity("origin-account", {}, 2048, true);
    const auto origin = dht::crypto::generateIdentity("origin-device", account, 2048, false);
    const auto peerAccount = dht::crypto::generateIdentity("fetch-account", {}, 2048, true);
    const auto peer = dht::crypto::generateIdentity("fetch-device", peerAccount, 2048, false);
    const auto circlePath = repoPath_ / "origin.git";
    auto created = CircleRepository::create(circlePath, origin);
    CPPUNIT_ASSERT(created && created->append(origin, InviteCircleMember {peerAccount.second->getId().toString()}));
    const auto circle = created->id();
    created.reset();
    CircleModule serving(origin, [](const auto&) { return true; });
    CircleModule fetching(peer, [](const auto&) { return true; });
    CPPUNIT_ASSERT(serving.mountCircle(circlePath, circle));
    const auto route = "circles/v1/" + circle;
    const auto download = [&](const std::filesystem::path& repository, std::string_view ref) {
        const auto server = std::make_shared<FakeChannelSocket>();
        const auto client = std::make_shared<PipeSocket>(server);
        const auto session = serving.open(route, peer.second, [server] { server->shutdown(); });
        if (!session)
            return std::optional<std::string>();
        GitServer gitServer(accountId_, circle, server, session->repository, [session] { return session->grant(); });
        return downloadCircleRoute(repository, "origin", route, client, ref);
    };
    const auto clonePath = repoPath_ / "clone.git";
    CPPUNIT_ASSERT(fetching.clone(route, clonePath, circle, download));
    CPPUNIT_ASSERT(fetching.circle(circle)->head() == serving.circle(circle)->head());
    const auto update = serving.circle(circle)->append(origin, SetCircleRole {"writer", {{Permission::Text}, {}}});
    CPPUNIT_ASSERT(update && fetching.fetch(route, download).head == *update);
    CPPUNIT_ASSERT(fetching.circle(circle)->policy() == serving.circle(circle)->policy());
    CPPUNIT_ASSERT(fetching.fetch(route, download).status == CircleFetchStatus::UNCHANGED);
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
