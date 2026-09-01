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

#include "../../test_runner.h"
#include "fileutils.h"
#include "jami.h"
#include "manager.h"
#include "sip/sipaccount_config.h"
#include "sip/sipaccountbase.h"

#include <msgpack.hpp>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <fstream>
#include <mutex>
#include <vector>

using namespace std::chrono_literals;

namespace jami::test {

class MessageEngineAccount final : public SIPAccountBase
{
public:
    struct Attempt
    {
        std::string peer;
        std::string device;
        uint64_t token;
    };

    explicit MessageEngineAccount(const std::string& accountId)
        : SIPAccountBase(accountId)
    {}

    std::unique_ptr<AccountConfig> buildConfig() const override
    {
        return std::make_unique<SipAccountConfig>(getAccountID());
    }
    std::string getFromUri() const override { return {}; }
    std::string_view getAccountType() const override { return "TEST"; }
    void doRegister() override {}
    void doUnregister(bool) override {}
    std::shared_ptr<Call> newOutgoingCall(std::string_view, const std::vector<libjami::MediaMap>&) override
    {
        return {};
    }
    std::shared_ptr<SIPCall> newIncomingCall(const std::string&,
                                             const std::vector<libjami::MediaMap>&,
                                             const std::shared_ptr<SipTransport>&) override
    {
        return {};
    }
    void updateProfile(const std::string&,
                       const std::string&,
                       const std::string&,
                       const std::string&,
                       int32_t) override
    {}
    bool isSrtpEnabled() const override { return false; }
    std::string getToUri(const std::string& username) const override { return username; }
    MatchRank matches(std::string_view, std::string_view) const override { return MatchRank::NONE; }
    std::string getUserUri() const override { return {}; }

    void sendMessage(const std::string& peer,
                     const std::string& device,
                     const std::map<std::string, std::string>&,
                     uint64_t token,
                     bool,
                     bool) override
    {
        if (completeWrites_)
            messageEngine_.onMessageSent(peer, token, true, device);
        std::lock_guard lock(mutex_);
        attempts_.emplace_back(Attempt {peer, device, token});
        condition_.notify_all();
    }

    void resume() { setRegistrationState(RegistrationState::REGISTERED); }
    void saveMessages() { messageEngine_.save(); }
    void completeWrites(bool complete) { completeWrites_ = complete; }
    size_t pendingMessages() const { return messageEngine_.pendingMessageCount(); }

    void sendFetched(const std::string& peer,
                     const std::string& device,
                     const std::string& conversation,
                     const std::string& commit)
    {
        messageEngine_.sendMessage(peer,
                                   device,
                                   {{"application/im-gitmessage-id", commit}},
                                   0,
                                   im::MessageDelivery {
                                       im::MessageCompletion::FETCHED, conversation, commit});
    }

    void acknowledgeDevice(const std::string& conversation,
                           const std::string& device,
                           const std::string& commit,
                           const im::MessageEngine::CommitCovered& covered)
    {
        messageEngine_.acknowledgeDeviceFetched(conversation, device, commit, covered);
    }

    void acknowledgeMember(const std::string& conversation,
                           const std::string& peer,
                           const std::string& commit,
                           const im::MessageEngine::CommitCovered& covered)
    {
        messageEngine_.acknowledgeMemberFetched(conversation, peer, commit, covered);
    }

    bool waitForAttempts(size_t count)
    {
        std::unique_lock lock(mutex_);
        return condition_.wait_for(lock, 5s, [&] { return attempts_.size() >= count; });
    }

    std::vector<Attempt> attempts() const
    {
        std::lock_guard lock(mutex_);
        return attempts_;
    }

    void completeAttempt(size_t index)
    {
        Attempt attempt;
        {
            std::lock_guard lock(mutex_);
            attempt = attempts_.at(index);
        }
        messageEngine_.onMessageSent(attempt.peer, attempt.token, true, attempt.device);
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::vector<Attempt> attempts_;
    std::atomic_bool completeWrites_ {false};
};

class MessageEngineTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "message_engine"; }

    void setUp() override;
    void tearDown() override;

private:
    void testDeviceQueueSurvivesRestart();
    void testLegacyQueueStillLoads();
    void testFetchedCompletion();
    void testInFlightRefreshIgnoresStaleCompletion();

    static constexpr auto ACCOUNT_ID = "message-engine-test";
    static constexpr auto LEGACY_ACCOUNT_ID = "message-engine-legacy-test";
    static constexpr auto FETCHED_ACCOUNT_ID = "message-engine-fetched-test";
    static constexpr auto REFRESH_ACCOUNT_ID = "message-engine-refresh-test";

    CPPUNIT_TEST_SUITE(MessageEngineTest);
    CPPUNIT_TEST(testDeviceQueueSurvivesRestart);
    CPPUNIT_TEST(testLegacyQueueStillLoads);
    CPPUNIT_TEST(testFetchedCompletion);
    CPPUNIT_TEST(testInFlightRefreshIgnoresStaleCompletion);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MessageEngineTest, MessageEngineTest::name());

void
MessageEngineTest::setUp()
{
    libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
    if (!Manager::instance().initialized)
        CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    dhtnet::fileutils::remove(fileutils::get_cache_dir() / ACCOUNT_ID, true);
    dhtnet::fileutils::remove(fileutils::get_cache_dir() / LEGACY_ACCOUNT_ID, true);
    dhtnet::fileutils::remove(fileutils::get_cache_dir() / FETCHED_ACCOUNT_ID, true);
    dhtnet::fileutils::remove(fileutils::get_cache_dir() / REFRESH_ACCOUNT_ID, true);
}

void
MessageEngineTest::tearDown()
{
    dhtnet::fileutils::remove(fileutils::get_cache_dir() / ACCOUNT_ID, true);
    dhtnet::fileutils::remove(fileutils::get_cache_dir() / LEGACY_ACCOUNT_ID, true);
    dhtnet::fileutils::remove(fileutils::get_cache_dir() / FETCHED_ACCOUNT_ID, true);
    dhtnet::fileutils::remove(fileutils::get_cache_dir() / REFRESH_ACCOUNT_ID, true);
}

void
MessageEngineTest::testDeviceQueueSurvivesRestart()
{
    const std::map<std::string, std::string> payload {{"application/im-gitmessage-id", "commit"}};
    {
        auto account = std::make_shared<MessageEngineAccount>(ACCOUNT_ID);
        account->sendTextMessage("peer", {}, payload);
        account->sendTextMessage("peer", "device", payload);
        CPPUNIT_ASSERT(account->waitForAttempts(2));
        account->saveMessages();
    }

    auto account = std::make_shared<MessageEngineAccount>(ACCOUNT_ID);
    account->resume();
    CPPUNIT_ASSERT(account->waitForAttempts(2));

    const auto attempts = account->attempts();
    CPPUNIT_ASSERT(std::any_of(attempts.begin(), attempts.end(), [](const auto& attempt) {
        return attempt.peer == "peer" && attempt.device.empty();
    }));
    CPPUNIT_ASSERT(std::any_of(attempts.begin(), attempts.end(), [](const auto& attempt) {
        return attempt.peer == "peer" && attempt.device == "device";
    }));
}

void
MessageEngineTest::testLegacyQueueStillLoads()
{
    const auto path = fileutils::get_cache_dir() / LEGACY_ACCOUNT_ID / "messages";
    {
        auto account = std::make_shared<MessageEngineAccount>(LEGACY_ACCOUNT_ID);
        account->sendTextMessage("peer", {}, {{"text/plain", "hello"}});
        CPPUNIT_ASSERT(account->waitForAttempts(1));
        account->saveMessages();
    }

    const auto data = fileutils::loadFile(path);
    msgpack::unpacker unpacker;
    unpacker.reserve_buffer(data.size());
    std::memcpy(unpacker.buffer(), data.data(), data.size());
    unpacker.buffer_consumed(data.size());
    msgpack::object_handle peerQueues;
    CPPUNIT_ASSERT(unpacker.next(peerQueues));
    {
        std::ofstream legacy(path, std::ios::trunc | std::ios::binary);
        msgpack::pack(legacy, peerQueues.get());
    }

    auto account = std::make_shared<MessageEngineAccount>(LEGACY_ACCOUNT_ID);
    account->resume();
    CPPUNIT_ASSERT(account->waitForAttempts(1));
    CPPUNIT_ASSERT(account->attempts().front().device.empty());
}

void
MessageEngineTest::testFetchedCompletion()
{
    constexpr auto conversation = "conversation";
    constexpr auto device = "device";
    {
        auto account = std::make_shared<MessageEngineAccount>(FETCHED_ACCOUNT_ID);
        account->completeWrites(true);
        account->sendFetched("peer", device, conversation, "old");
        CPPUNIT_ASSERT(account->waitForAttempts(1));
        CPPUNIT_ASSERT_EQUAL(size_t(1), account->pendingMessages());
        account->saveMessages();
    }

    auto account = std::make_shared<MessageEngineAccount>(FETCHED_ACCOUNT_ID);
    CPPUNIT_ASSERT_EQUAL(size_t(1), account->pendingMessages());
    account->completeWrites(true);
    account->sendFetched("peer", device, conversation, "new");
    CPPUNIT_ASSERT(account->waitForAttempts(1));
    CPPUNIT_ASSERT_EQUAL(size_t(1), account->pendingMessages());

    const auto exact = [](const auto& advertised, const auto& fetched) { return advertised == fetched; };
    account->acknowledgeDevice(conversation, device, "old", exact);
    CPPUNIT_ASSERT_EQUAL(size_t(1), account->pendingMessages());
    account->acknowledgeDevice(conversation, device, "new", exact);
    CPPUNIT_ASSERT_EQUAL(size_t(0), account->pendingMessages());

    account->sendFetched("peer", "device-a", conversation, "member-head");
    account->sendFetched("peer", "device-b", conversation, "member-head");
    CPPUNIT_ASSERT(account->waitForAttempts(3));
    CPPUNIT_ASSERT_EQUAL(size_t(2), account->pendingMessages());
    account->acknowledgeMember(conversation, "peer", "descendant", [](const auto& advertised, const auto& fetched) {
        return advertised == "member-head" && fetched == "descendant";
    });
    CPPUNIT_ASSERT_EQUAL(size_t(0), account->pendingMessages());

    account->sendFetched("peer", {}, conversation, "account-head");
    CPPUNIT_ASSERT(account->waitForAttempts(4));
    CPPUNIT_ASSERT_EQUAL(size_t(1), account->pendingMessages());
    account->acknowledgeMember(conversation, "peer", "account-head", exact);
    CPPUNIT_ASSERT_EQUAL(size_t(0), account->pendingMessages());

    account->sendTextMessage("peer", {}, {{"text/plain", "done on write"}});
    CPPUNIT_ASSERT(account->waitForAttempts(5));
    CPPUNIT_ASSERT_EQUAL(size_t(0), account->pendingMessages());
}

void
MessageEngineTest::testInFlightRefreshIgnoresStaleCompletion()
{
    auto account = std::make_shared<MessageEngineAccount>(REFRESH_ACCOUNT_ID);
    account->sendFetched("peer", "device", "conversation", "old");
    CPPUNIT_ASSERT(account->waitForAttempts(1));

    account->sendFetched("peer", "device", "conversation", "new");
    CPPUNIT_ASSERT(account->waitForAttempts(2));
    const auto attempts = account->attempts();
    CPPUNIT_ASSERT(attempts[0].token != attempts[1].token);

    account->completeAttempt(0);
    CPPUNIT_ASSERT_EQUAL(size_t(1), account->pendingMessages());
    account->acknowledgeDevice(
        "conversation", "device", "old", [](const auto& advertised, const auto& fetched) {
            return advertised == fetched;
        });
    CPPUNIT_ASSERT_EQUAL(size_t(1), account->pendingMessages());

    account->completeAttempt(1);
    account->acknowledgeDevice(
        "conversation", "device", "new", [](const auto& advertised, const auto& fetched) {
            return advertised == fetched;
        });
    CPPUNIT_ASSERT_EQUAL(size_t(0), account->pendingMessages());
}

} // namespace jami::test

CORE_TEST_RUNNER(jami::test::MessageEngineTest::name())