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
#include "jamidht/message_channel_handler.h"
#include "manager.h"
#include "sip/sipaccount_config.h"
#include "sip/sipaccountbase.h"

#include <dhtnet/certstore.h>
#include <dhtnet/connectionmanager.h>
#include <dhtnet/ice_transport_factory.h>
#include <opendht/crypto.h>
#include <opendht/dhtrunner.h>
#include <msgpack.hpp>

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
    void updateProfile(const std::string&, const std::string&, const std::string&, const std::string&, int32_t) override
    {}
    bool isSrtpEnabled() const override { return false; }
    std::string getToUri(const std::string& username) const override { return username; }
    MatchRank matches(std::string_view, std::string_view) const override { return MatchRank::NONE; }
    std::string getUserUri() const override { return {}; }

    void sendMessage(const std::string& peer,
                     const std::string& device,
                     const std::map<std::string, std::string>&,
                     uint64_t,
                     bool,
                     bool) override
    {
        std::lock_guard lock(mutex_);
        attempts_.emplace_back(Attempt {peer, device});
        condition_.notify_all();
    }

    void resume() { setRegistrationState(RegistrationState::REGISTERED); }
    void saveMessages() { messageEngine_.save(); }

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

private:
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::vector<Attempt> attempts_;
};

class MessageEngineTest : public CppUnit::TestFixture
{
public:
    ~MessageEngineTest() { libjami::fini(); }
    static std::string name() { return "message_engine"; }

    void setUp() override;
    void tearDown() override;

private:
    void testDeviceQueueSurvivesRestart();
    void testLegacyQueueStillLoads();
    void testMessageChannelHandlerChainsPendingCallbacks();

    static constexpr auto ACCOUNT_ID = "message-engine-test";
    static constexpr auto LEGACY_ACCOUNT_ID = "message-engine-legacy-test";

    CPPUNIT_TEST_SUITE(MessageEngineTest);
    CPPUNIT_TEST(testDeviceQueueSurvivesRestart);
    CPPUNIT_TEST(testLegacyQueueStillLoads);
    CPPUNIT_TEST(testMessageChannelHandlerChainsPendingCallbacks);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MessageEngineTest, MessageEngineTest::name());

void
MessageEngineTest::setUp()
{
    libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
    if (!Manager::instance().initialized)
        CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    dhtnet::fileutils::removeAll(fileutils::get_cache_dir() / ACCOUNT_ID, true);
    dhtnet::fileutils::removeAll(fileutils::get_cache_dir() / LEGACY_ACCOUNT_ID, true);
}

void
MessageEngineTest::tearDown()
{
    dhtnet::fileutils::removeAll(fileutils::get_cache_dir() / ACCOUNT_ID, true);
    dhtnet::fileutils::removeAll(fileutils::get_cache_dir() / LEGACY_ACCOUNT_ID, true);
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
MessageEngineTest::testMessageChannelHandlerChainsPendingCallbacks()
{
    auto ca = dht::crypto::generateIdentity("ca");
    auto aliceAccount = dht::crypto::generateIdentity("alice-acc", ca, 2048, true);
    auto bobAccount = dht::crypto::generateIdentity("bob-acc", ca, 2048, true);
    auto aliceDevice = dht::crypto::generateIdentity("alice-dev", aliceAccount);
    auto bobDevice = dht::crypto::generateIdentity("bob-dev", bobAccount);
    auto bobDeviceId = bobDevice.second->getLongId();

    auto cacheDir = fileutils::get_cache_dir() / ACCOUNT_ID;
    auto certStore = std::make_shared<dhtnet::tls::CertificateStore>(cacheDir / "certs", nullptr);
    certStore->pinCertificate(bobDevice.second);

    auto dht = std::make_shared<dht::DhtRunner>();
    dht::DhtRunner::Config dhtConfig;
    dhtConfig.dht_config.id = aliceDevice;
    dhtConfig.threaded = true;
    dht->run(dhtConfig, {});

    auto config = std::make_shared<dhtnet::ConnectionManager::Config>();
    config->id = aliceDevice;
    config->dht = dht;
    config->certStore = certStore;
    config->ioContext = Manager::instance().ioContext();
    config->factory = Manager::instance().getIceTransportFactory();
    config->cachePath = cacheDir / "cm";
    config->legacyMode = dhtnet::LegacyMode::Disabled;

    auto cm = std::make_unique<dhtnet::ConnectionManager>(config);
    cm->dhtStarted();

    MessageChannelHandler handler(
        *cm,
        [](const auto&, std::string&, const std::string&) {},
        [](const std::string&, bool) {},
        [](const std::string&, const DeviceId&) {});

    std::mutex mtx;
    std::condition_variable cv;
    bool firstCalled = false;
    bool secondCalled = false;

    handler.connect(
        bobDeviceId,
        "",
        [&](const std::shared_ptr<dhtnet::ChannelSocket>&, const DeviceId&) {
            std::lock_guard lk(mtx);
            firstCalled = true;
            cv.notify_all();
        },
        "");

    const auto channelName = "msg:" + bobDeviceId.toString();
    auto deadline = std::chrono::steady_clock::now() + 5s;
    while (!cm->isConnecting(bobDeviceId, channelName) && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(10ms);
    }
    CPPUNIT_ASSERT(cm->isConnecting(bobDeviceId, channelName));

    handler.connect(
        bobDeviceId,
        "",
        [&](const std::shared_ptr<dhtnet::ChannelSocket>&, const DeviceId&) {
            std::lock_guard lk(mtx);
            secondCalled = true;
            cv.notify_all();
        },
        "application/im-gitmessage-id");

    cm.reset();
    dht->join();

    std::unique_lock lk(mtx);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return firstCalled && secondCalled; }));
}

} // namespace jami::test

CORE_TEST_RUNNER(jami::test::MessageEngineTest::name())