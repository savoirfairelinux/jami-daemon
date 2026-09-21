/*
 * Copyright (C) 2026 Savoir-faire Linux Inc.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "../../test_runner.h"
#include "common.h"
#include "jami.h"
#include "account_const.h"
#include "manager.h"
#include "fileutils.h"
#include "jamidht/jamiaccount.h"
#include "jamidht/conversation_module.h"
#include "jamidht/conversationrepository.h"
#include "jamidht/commit_message.h"
#include "jamidht/feed_module.h"

#include <cppunit/TestAssert.h>
#include <cppunit/extensions/HelperMacros.h>
#include <chrono>
#include <functional>
#include <future>
#include <thread>

using namespace std::chrono_literals;

namespace jami::test {

struct PreviousSyncMsg
{
    DeviceSync ds;
    decltype(SyncMsg::c) c;
    decltype(SyncMsg::cr) cr;
    decltype(SyncMsg::p) p;
    decltype(SyncMsg::ld) ld;
    decltype(SyncMsg::ms) ms;
    MSGPACK_DEFINE(ds, c, cr, p, ld, ms)
};

class FeedTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "Feed"; }
    ~FeedTest() { libjami::fini(); }
    void setUp() override
    {
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (!Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
        auto actors = load_actors_and_wait_for_announcement("actors/alice-bob-carla.yml");
        alice = actors.at("alice");
        bob = actors.at("bob");
        carla = actors.at("carla");
        for (const auto& sender : {alice, bob, carla})
            for (const auto& receiver : {alice, bob, carla})
                if (sender != receiver)
                    account(sender)->addContact(account(receiver)->getUsername(), true);
    }
    void tearDown() override
    {
        if (bob2.empty())
            wait_for_removal_of({alice, bob, carla});
        else
            wait_for_removal_of({alice, bob, carla, bob2});
        std::filesystem::remove(std::filesystem::current_path() / "feed-bob.gz");
    }

    void testFeedLifecycle()
    {
        auto id = libjami::createFeed(alice, "Announcements", "", false);
        CPPUNIT_ASSERT(!id.empty());
        CPPUNIT_ASSERT(poll([&] { return libjami::conversationInfos(alice, id)["title"] == "Announcements"; }));
        libjami::refreshFeeds(bob);
        std::this_thread::sleep_for(500ms);
        CPPUNIT_ASSERT(entry(bob, id).empty());

        CPPUNIT_ASSERT(libjami::setFeedAccess(alice, id, account(bob)->getUsername(), true));
        CPPUNIT_ASSERT(poll([&] { return entry(bob, id)["available"] == "true"; }));
        CPPUNIT_ASSERT(!account(bob)->convModule()->getConversation(id));
        libjami::refreshFeeds(carla);
        std::this_thread::sleep_for(500ms);
        CPPUNIT_ASSERT(entry(carla, id).empty());
        CPPUNIT_ASSERT(!libjami::subscribeFeed(carla, id, true));

        CPPUNIT_ASSERT(libjami::subscribeFeed(bob, id, true));
        CPPUNIT_ASSERT(poll([&] { return entry(bob, id)["subscribed"] == "true"; }));
        CPPUNIT_ASSERT(poll(
            [&] { return account(alice)->convModule()->getConversation(id)->isMember(account(bob)->getUsername()); }));
        auto ownerRepo = std::make_unique<ConversationRepository>(account(alice), id);
        auto memberRepo = std::make_unique<ConversationRepository>(account(bob), id);
        auto post = publish(alice, id, CommitMessage::text("Owner publication"));
        CPPUNIT_ASSERT(!post.empty());
        CPPUNIT_ASSERT(poll([&] { return memberRepo->hasCommit(post); }));
        CPPUNIT_ASSERT(memberRepo->commitMessage(CommitMessage::text("Forbidden publication").toString()).empty());
        CPPUNIT_ASSERT(memberRepo->commitMessage(CommitMessage::text("Read-only reply", post).toString()).empty());
        CPPUNIT_ASSERT(memberRepo->updateInfos({{"title", "Hijacked"}}).empty());
        CPPUNIT_ASSERT(!libjami::setFeedAccess(bob, id, account(carla)->getUsername(), true));

        CPPUNIT_ASSERT(libjami::updateFeed(alice, id, {{"feedReplies", "true"}}));
        CPPUNIT_ASSERT(poll([&] { return memberRepo->infos()["feedReplies"] == "true"; }));
        auto reply = publish(bob, id, CommitMessage::text("Subscriber reply", post));
        CPPUNIT_ASSERT(!reply.empty());
        CPPUNIT_ASSERT(memberRepo->commitMessage(CommitMessage::text("Still not a publication").toString()).empty());
        CPPUNIT_ASSERT(memberRepo->commitMessage(CommitMessage::text("Nested reply", reply).toString()).empty());

        ownerRepo.reset();
        memberRepo.reset();
        CPPUNIT_ASSERT(libjami::subscribeFeed(bob, id, false));
        CPPUNIT_ASSERT(poll([&] { return entry(bob, id)["subscribed"] == "false"; }));
        CPPUNIT_ASSERT(entry(bob, id)["available"] == "true");
        CPPUNIT_ASSERT(poll(
            [&] { return !account(alice)->convModule()->getConversation(id)->isMember(account(bob)->getUsername()); }));
        CPPUNIT_ASSERT(libjami::subscribeFeed(bob, id, true));
        CPPUNIT_ASSERT(poll([&] { return entry(bob, id)["subscribed"] == "true"; }));
        CPPUNIT_ASSERT(libjami::setFeedAccess(alice, id, account(bob)->getUsername(), false));
        CPPUNIT_ASSERT(poll([&] { return entry(bob, id)["available"] == "false"; }));
        CPPUNIT_ASSERT(!libjami::subscribeFeed(bob, id, true));
        CPPUNIT_ASSERT(libjami::setFeedAccess(alice, id, account(bob)->getUsername(), true));
        CPPUNIT_ASSERT(poll([&] { return entry(bob, id)["available"] == "true"; }));
        CPPUNIT_ASSERT(entry(bob, id)["subscribed"] == "false");
        CPPUNIT_ASSERT(entry(bob, id)["requested"] == "false");
        CPPUNIT_ASSERT(libjami::subscribeFeed(bob, id, true));
        CPPUNIT_ASSERT(poll([&] { return entry(bob, id)["subscribed"] == "true"; }));
        CPPUNIT_ASSERT(libjami::updateFeed(alice, id, {{"feedClosed", "true"}}));
        CPPUNIT_ASSERT(poll([&] { return entry(alice, id)["feedClosed"] == "true"; }));
    }

    void testFeedDeviceSubscriptions()
    {
        const auto archive = std::filesystem::current_path() / "feed-bob.gz";
        CPPUNIT_ASSERT(account(bob)->exportArchive(archive.string()));
        auto config = libjami::getAccountTemplate("RING");
        config[libjami::Account::ConfProperties::ARCHIVE_PATH] = archive.string();
        config[libjami::Account::ConfProperties::ARCHIVE_PASSWORD] = "";
        config[libjami::Account::ConfProperties::DISPLAYNAME] = "Feed second device";
        bob2 = Manager::instance().addAccount(config);
        wait_for_announcement_of(bob2);
        auto id = libjami::createFeed(alice, "Device sync", "", true);
        CPPUNIT_ASSERT(poll([&] { return entry(alice, id)["title"] == "Device sync"; }));
        CPPUNIT_ASSERT(libjami::setFeedAccess(alice, id, account(bob)->getUsername(), true));
        CPPUNIT_ASSERT(poll([&] { return entry(bob, id)["available"] == "true"; }));
        CPPUNIT_ASSERT(poll([&] { return entry(bob2, id)["available"] == "true"; }));
        CPPUNIT_ASSERT(libjami::subscribeFeed(bob, id, true));
        CPPUNIT_ASSERT(poll([&] { return entry(bob, id)["subscribed"] == "true"; }));
        CPPUNIT_ASSERT(poll([&] { return entry(bob2, id)["subscribed"] == "true"; }));
        // Generic leave actions must also preserve the account-wide choice.
        CPPUNIT_ASSERT(libjami::removeConversation(bob2, id));
        CPPUNIT_ASSERT(poll([&] { return entry(bob, id)["subscribed"] == "false"; }));
        CPPUNIT_ASSERT(poll([&] { return entry(bob2, id)["subscribed"] == "false"; }));
        CPPUNIT_ASSERT(entry(bob, id)["available"] == "true");
        CPPUNIT_ASSERT(entry(bob2, id)["available"] == "true");
        CPPUNIT_ASSERT(poll(
            [&] { return !account(alice)->convModule()->getConversation(id)->isMember(account(bob)->getUsername()); }));
        CPPUNIT_ASSERT(libjami::subscribeFeed(bob, id, true));
        CPPUNIT_ASSERT(poll([&] { return entry(bob, id)["subscribed"] == "true"; }));
        CPPUNIT_ASSERT(poll([&] { return entry(bob2, id)["subscribed"] == "true"; }));
    }

    void testCataloguePersistenceAndCancellation()
    {
        auto first = libjami::createFeed(alice, "First", "", false);
        auto second = libjami::createFeed(alice, "Second", "", true);
        CPPUNIT_ASSERT(!first.empty() && !second.empty() && first != second);
        CPPUNIT_ASSERT(poll([&] { return entry(alice, second)["title"] == "Second"; }));
        CPPUNIT_ASSERT(libjami::setFeedAccess(alice, first, account(bob)->getUsername(), true));
        CPPUNIT_ASSERT(libjami::setFeedAccess(alice, second, account(carla)->getUsername(), true));
        CPPUNIT_ASSERT(poll([&] { return entry(bob, first)["available"] == "true"; }));
        CPPUNIT_ASSERT(poll([&] { return entry(carla, second)["available"] == "true"; }));
        CPPUNIT_ASSERT(entry(bob, second).empty());
        CPPUNIT_ASSERT(entry(carla, first).empty());
        auto restored = std::make_shared<FeedModule>(account(bob));
        const auto catalogue = restored->list();
        CPPUNIT_ASSERT(
            std::any_of(catalogue.begin(), catalogue.end(), [&](const auto& item) { return item.at("id") == first; }));
        CPPUNIT_ASSERT(libjami::updateFeed(alice, first, {{"title", "Renamed"}}));
        CPPUNIT_ASSERT(poll([&] { return entry(bob, first)["title"] == "Renamed"; }));

        Manager::instance().sendRegister(alice, false);
        std::this_thread::sleep_for(500ms);
        CPPUNIT_ASSERT(libjami::subscribeFeed(bob, first, true));
        CPPUNIT_ASSERT(libjami::subscribeFeed(bob, first, false));
        CPPUNIT_ASSERT(account(bob)->feeds()->subscriptionCancelled(first));
        Manager::instance().sendRegister(alice, true);
        wait_for_announcement_of(alice);
        std::this_thread::sleep_for(2s);
        CPPUNIT_ASSERT(entry(bob, first)["subscribed"] == "false");
        CPPUNIT_ASSERT(!account(bob)->convModule()->getConversation(first));
        CPPUNIT_ASSERT(libjami::updateFeed(alice, first, {{"feedClosed", "true"}}));
        CPPUNIT_ASSERT(poll([&] { return entry(bob, first)["available"] == "false"; }));
    }

    void testAccountSyncCompatibility()
    {
        SyncMsg current;
        const std::string id(40, 'a');
        current.feeds[id] = {{"title", "Feed"}, {"feedOwner", account(alice)->getUsername()}};
        current.c[id] = ConvInfo(id);
        msgpack::sbuffer buffer;
        msgpack::pack(buffer, current);
        const auto old = msgpack::unpack(buffer.data(), buffer.size()).get().as<PreviousSyncMsg>();
        CPPUNIT_ASSERT(old.c.count(id));
        buffer.clear();
        msgpack::pack(buffer, old);
        const auto restored = msgpack::unpack(buffer.data(), buffer.size()).get().as<SyncMsg>();
        CPPUNIT_ASSERT(restored.c.count(id));
        CPPUNIT_ASSERT(restored.feeds.empty());
    }

    void testUnsubscribeWaitsForLeaveAcknowledgement()
    {
        auto id = libjami::createFeed(alice, "Leave receipt", "", false);
        CPPUNIT_ASSERT(poll([&] { return entry(alice, id)["title"] == "Leave receipt"; }));
        CPPUNIT_ASSERT(libjami::setFeedAccess(alice, id, account(bob)->getUsername(), true));
        CPPUNIT_ASSERT(poll([&] { return entry(bob, id)["available"] == "true"; }));
        CPPUNIT_ASSERT(libjami::subscribeFeed(bob, id, true));
        CPPUNIT_ASSERT(poll([&] { return entry(bob, id)["subscribed"] == "true"; }));
        auto subscriber = account(bob);
        auto* cm = subscriber->convModule();
        auto conversation = cm->getConversation(id);
        auto previous = conversation->lastCommitId();
        const auto ownerDevice = std::string(account(alice)->currentDeviceId());
        Manager::instance().sendRegister(alice, false);
        Manager::instance().sendRegister(bob, false);
        CPPUNIT_ASSERT(libjami::subscribeFeed(bob, id, false));
        ConversationRepository repository(subscriber, id);
        const auto leave = repository.getHead();
        CPPUNIT_ASSERT(leave != previous);
        cm->setFetched(id, ownerDevice, previous);
        CPPUNIT_ASSERT(std::filesystem::is_directory(fileutils::get_data_dir() / bob / "conversations" / id));
        cm->setFetched(id, ownerDevice, leave);
        CPPUNIT_ASSERT(!std::filesystem::exists(fileutils::get_data_dir() / bob / "conversations" / id));
    }

private:
    std::string publish(const std::string& acc, const std::string& id, CommitMessage message)
    {
        auto promise = std::make_shared<std::promise<std::string>>();
        auto future = promise->get_future();
        account(acc)->convModule()->createCommit(id,
                                                 std::move(message),
                                                 true,
                                                 {},
                                                 [promise](bool ok, const std::string& commit) {
                                                     promise->set_value(ok ? commit : "");
                                                 });
        CPPUNIT_ASSERT(future.wait_for(10s) == std::future_status::ready);
        return future.get();
    }
    std::shared_ptr<JamiAccount> account(const std::string& id)
    {
        return Manager::instance().getAccount<JamiAccount>(id);
    }
    std::map<std::string, std::string> entry(const std::string& owner, const std::string& id)
    {
        for (const auto& item : libjami::getFeeds(owner))
            if (item.at("id") == id)
                return item;
        return {};
    }
    bool poll(const std::function<bool()>& predicate)
    {
        const auto deadline = std::chrono::steady_clock::now() + 40s;
        while (std::chrono::steady_clock::now() < deadline) {
            if (predicate())
                return true;
            std::this_thread::sleep_for(100ms);
        }
        return predicate();
    }
    std::string alice, bob, carla, bob2;
    CPPUNIT_TEST_SUITE(FeedTest);
    CPPUNIT_TEST(testFeedLifecycle);
    CPPUNIT_TEST(testFeedDeviceSubscriptions);
    CPPUNIT_TEST(testCataloguePersistenceAndCancellation);
    CPPUNIT_TEST(testAccountSyncCompatibility);
    CPPUNIT_TEST(testUnsubscribeWaitsForLeaveAcknowledgement);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(FeedTest, FeedTest::name());
} // namespace jami::test

int
main(int argc, char** argv)
{
    CppUnit::TextUi::TestRunner runner;
    runner.addTest(CppUnit::TestFactoryRegistry::getRegistry(jami::test::FeedTest::name()).makeTest());
    return runner.run(argc > 1 ? argv[1] : "") ? 0 : 1;
}
