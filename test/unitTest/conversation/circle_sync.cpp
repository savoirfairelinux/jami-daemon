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
#include "account_const.h"
#include "circle_interface.h"
#include "common.h"
#include "conversation_interface.h"
#include "jami.h"
#include "jamidht/circle_module.h"
#include "jamidht/circle_service.h"
#include "jamidht/jamiaccount.h"
#include "manager.h"

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <future>
#include <mutex>
#include <set>
#include <thread>

using namespace std::literals::chrono_literals;
using namespace libjami::Account;

namespace jami {
namespace test {

namespace {

// Circles have no client signals yet, so tests watch their state.
bool
waitUntil(const std::function<bool()>& condition, std::chrono::seconds timeout = 60s)
{
    const auto end = std::chrono::steady_clock::now() + timeout;
    while (!condition()) {
        if (std::chrono::steady_clock::now() > end)
            return false;
        std::this_thread::sleep_for(100ms);
    }
    return true;
}

bool
waitDone(const std::function<void(CircleService::Done)>& operation, std::chrono::seconds timeout = 60s)
{
    auto result = std::make_shared<std::promise<bool>>();
    auto future = result->get_future();
    operation([result](bool done) { result->set_value(done); });
    return future.wait_for(timeout) == std::future_status::ready && future.get();
}

} // namespace

class CircleSyncTest : public CppUnit::TestFixture
{
public:
    ~CircleSyncTest() { libjami::fini(); }
    static std::string name() { return "CircleSync"; }
    void setUp();
    void tearDown();

private:
    std::string aliceId;
    std::string bobId;
    std::string alice2Id;

    std::shared_ptr<JamiAccount> account(const std::string& id) const
    {
        return Manager::instance().getAccount<JamiAccount>(id);
    }
    std::shared_ptr<CircleService> circles(const std::string& id) const { return account(id)->circleService(); }
    std::optional<CircleMemberState> stateOf(const std::string& accountId,
                                             const std::string& circle,
                                             const std::string& member) const;
    // A circle of Alice's in which Bob is active and members may read and write channels.
    std::string circleWithBob();

    void testInviteAndJoin();
    void testChannelMessages();
    void testLeaveIsServed();
    void testOwnDeviceSync();
    void testPublicApi();

    CPPUNIT_TEST_SUITE(CircleSyncTest);
    CPPUNIT_TEST(testInviteAndJoin);
    CPPUNIT_TEST(testChannelMessages);
    CPPUNIT_TEST(testLeaveIsServed);
    CPPUNIT_TEST(testOwnDeviceSync);
    CPPUNIT_TEST(testPublicApi);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(CircleSyncTest, CircleSyncTest::name());

void
CircleSyncTest::setUp()
{
    libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
    if (not Manager::instance().initialized)
        CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));

    auto actors = load_actors("actors/alice-bob.yml");
    aliceId = actors["alice"];
    bobId = actors["bob"];
    alice2Id.clear();
    wait_for_announcement_of({aliceId, bobId});
    add_confirmed_contact(aliceId, bobId);
}

void
CircleSyncTest::tearDown()
{
    libjami::unregisterSignalHandlers();
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
    std::remove(aliceArchive.c_str());
    if (alice2Id.empty())
        wait_for_removal_of({aliceId, bobId});
    else
        wait_for_removal_of({aliceId, bobId, alice2Id});
}

std::optional<CircleMemberState>
CircleSyncTest::stateOf(const std::string& accountId, const std::string& circle, const std::string& member) const
{
    const auto repository = circles(accountId)->repository(circle);
    if (!repository)
        return std::nullopt;
    const auto policy = repository->policy();
    const auto found = policy.members.find(member);
    if (found == policy.members.end())
        return std::nullopt;
    return found->second.state;
}

std::string
CircleSyncTest::circleWithBob()
{
    const auto aliceCircles = circles(aliceId);
    const auto bobCircles = circles(bobId);
    const auto bobUri = account(bobId)->getUsername();

    const auto circle = aliceCircles->create(
        PermissionRule {{Permission::ReadChannel, Permission::Text, Permission::Reaction, Permission::Edit}, {}});
    CPPUNIT_ASSERT(circle);
    CPPUNIT_ASSERT(aliceCircles->invite(*circle, bobUri));
    CPPUNIT_ASSERT(waitUntil([&] { return bobCircles->requests().contains(*circle); }));
    CPPUNIT_ASSERT(waitDone([&](auto done) { bobCircles->acceptRequest(*circle, std::move(done)); }));
    CPPUNIT_ASSERT(waitUntil([&] { return stateOf(aliceId, *circle, bobUri) == CircleMemberState::ACTIVE; }));
    return *circle;
}

void
CircleSyncTest::testInviteAndJoin()
{
    const auto circle = circleWithBob();
    const auto bobUri = account(bobId)->getUsername();
    const auto bobCircles = circles(bobId);

    CPPUNIT_ASSERT(circles(aliceId)->circles().contains(circle));
    CPPUNIT_ASSERT(bobCircles->circles().contains(circle));
    CPPUNIT_ASSERT(!bobCircles->requests().contains(circle));
    CPPUNIT_ASSERT(stateOf(bobId, circle, bobUri) == CircleMemberState::ACTIVE);
    // Joining enrolls the joining device, so that it can sign.
    CPPUNIT_ASSERT(bobCircles->repository(circle)->enrolled(account(bobId)->currentDeviceId()));
}

void
CircleSyncTest::testChannelMessages()
{
    const auto circle = circleWithBob();
    const auto aliceCircles = circles(aliceId);
    const auto bobCircles = circles(bobId);

    const auto channel = aliceCircles->createChannel(circle);
    CPPUNIT_ASSERT(channel);
    CPPUNIT_ASSERT(waitUntil([&] {
        const auto repository = bobCircles->repository(circle);
        return repository && repository->policy().channels.contains(*channel);
    }));
    const std::string aliceDevice(account(aliceId)->currentDeviceId());
    CPPUNIT_ASSERT(
        waitDone([&](auto done) { bobCircles->joinChannel(circle, *channel, std::move(done), aliceDevice); }));
    CPPUNIT_ASSERT(bobCircles->circles().at(circle).channels.contains(*channel));

    const auto route = CircleModule::channelRoute(*channel);
    const auto heads = [&](const std::optional<std::string>& head) {
        return head && waitUntil([&] {
                   return account(aliceId)->circleModule()->head(route) == *head
                          && account(bobId)->circleModule()->head(route) == *head;
               });
    };
    const auto hello = aliceCircles->sendMessage(*channel, CommitMessage::text("hello"));
    CPPUNIT_ASSERT(heads(hello));
    CPPUNIT_ASSERT(heads(bobCircles->sendMessage(*channel, CommitMessage::text("hi"))));
    CPPUNIT_ASSERT(heads(bobCircles->sendMessage(*channel, CommitMessage::reaction("+1", *hello))));
    CPPUNIT_ASSERT(heads(aliceCircles->sendMessage(*channel, CommitMessage::edit("hello!", *hello))));
    // Only the author edits a message.
    CPPUNIT_ASSERT(!bobCircles->sendMessage(*channel, CommitMessage::edit("bye", *hello)));

    // Clients get the history as they do for a conversation, whether it was written or fetched.
    for (const auto& circles : {aliceCircles, bobCircles}) {
        CPPUNIT_ASSERT(circles->isChannel(*channel) && !circles->isChannel(circle));
        circles->clearCache(*channel);
        const auto page = circles->loadMessages(*channel, {}, {}, 2);
        CPPUNIT_ASSERT(page.size() == 2);
        CPPUNIT_ASSERT(page[0].body.at("body") == "hi" && page[0].linearizedParent == *hello);
        CPPUNIT_ASSERT_EQUAL(account(bobId)->getUsername(), page[0].body.at("author"));
        CPPUNIT_ASSERT(page[1].id == *hello && page[1].body.at("body") == "hello!");
        CPPUNIT_ASSERT(page[1].editions.size() == 1 && page[1].editions.front().at("body") == "hello");
        CPPUNIT_ASSERT(page[1].reactions.size() == 1 && page[1].reactions.front().at("body") == "+1");
        const auto rest = circles->loadMessages(*channel, *hello);
        CPPUNIT_ASSERT(rest.size() == 1 && rest[0].id == *channel && rest[0].type == "initial");
    }
}

void
CircleSyncTest::testLeaveIsServed()
{
    const auto circle = circleWithBob();
    const auto bobUri = account(bobId)->getUsername();
    const auto bobCircles = circles(bobId);

    CPPUNIT_ASSERT(bobCircles->leave(circle));
    CPPUNIT_ASSERT(bobCircles->circles().at(circle).departed);
    // Bob serves his leave until Alice fetched it, then forgets the circle.
    CPPUNIT_ASSERT(waitUntil([&] { return stateOf(aliceId, circle, bobUri) == CircleMemberState::LEFT; }));
    CPPUNIT_ASSERT(waitUntil([&] { return !bobCircles->circles().contains(circle); }));
}

void
CircleSyncTest::testOwnDeviceSync()
{
    const auto circle = circleWithBob();
    const auto aliceAccount = account(aliceId);

    const auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
    aliceAccount->exportArchive(aliceArchive);
    auto details = libjami::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE2";
    details[ConfProperties::ALIAS] = "ALICE2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PATH] = aliceArchive;
    alice2Id = Manager::instance().addAccount(details);
    wait_for_announcement_of(alice2Id);

    // The new device clones the circle from the other one and enrolls itself.
    CPPUNIT_ASSERT(waitUntil([&] { return circles(alice2Id)->circles().contains(circle); }, 120s));
    const std::string alice2Device(account(alice2Id)->currentDeviceId());
    CPPUNIT_ASSERT(waitUntil([&] { return circles(aliceId)->repository(circle)->enrolled(alice2Device); }, 120s));
}

void
CircleSyncTest::testPublicApi()
{
    const auto bobUri = account(bobId)->getUsername();

    // Shared, so that late signals after the test returned do not touch its frame.
    struct Events
    {
        std::mutex mutex;
        std::condition_variable cv;
        std::set<std::string> ready, removed, requests, channels, errors;
        std::vector<libjami::SwarmMessage> bobMessages, loaded;
        std::vector<std::string> permissions;
        uint32_t loadId {0};
    };
    auto events = std::make_shared<Events>();
    auto record = [events](auto&& update) {
        std::lock_guard lock(events->mutex);
        update(*events);
        events->cv.notify_all();
    };
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> handlers;
    handlers.insert(libjami::exportable_callback<libjami::CircleSignal::CircleReady>(
        [=](const std::string& accountId, const std::string& circle) {
            record([&](Events& e) { e.ready.emplace(accountId + circle); });
        }));
    handlers.insert(libjami::exportable_callback<libjami::CircleSignal::CircleRemoved>(
        [=](const std::string& accountId, const std::string& circle) {
            record([&](Events& e) { e.removed.emplace(accountId + circle); });
        }));
    handlers.insert(libjami::exportable_callback<libjami::CircleSignal::CircleRequestReceived>(
        [=](const std::string& accountId, const std::string& circle, const std::map<std::string, std::string>&) {
            record([&](Events& e) { e.requests.emplace(accountId + circle); });
        }));
    handlers.insert(libjami::exportable_callback<libjami::CircleSignal::CircleChannelReady>(
        [=](const std::string& accountId, const std::string&, const std::string& channel) {
            record([&](Events& e) { e.channels.emplace(accountId + channel); });
        }));
    handlers.insert(libjami::exportable_callback<libjami::ConversationSignal::SwarmMessageReceived>(
        [=, bob = bobId](const std::string& accountId, const std::string&, const libjami::SwarmMessage& message) {
            if (accountId == bob)
                record([&](Events& e) { e.bobMessages.emplace_back(message); });
        }));
    handlers.insert(libjami::exportable_callback<libjami::ConversationSignal::SwarmLoaded>(
        [=](uint32_t id,
            const std::string&,
            const std::string&,
            const std::vector<libjami::SwarmMessage>& messages,
            const std::vector<std::string>& permissions) {
            record([&](Events& e) {
                e.loadId = id;
                e.loaded = messages;
                e.permissions = permissions;
            });
        }));
    handlers.insert(libjami::exportable_callback<libjami::ConversationSignal::OnConversationError>(
        [=](const std::string& accountId, const std::string& conversation, int, const std::string&) {
            record([&](Events& e) { e.errors.emplace(accountId + conversation); });
        }));
    libjami::registerSignalHandlers(handlers);
    auto wait = [&](auto&& condition) {
        std::unique_lock lock(events->mutex);
        return events->cv.wait_for(lock, 60s, [&] { return condition(*events); });
    };

    const auto circle = libjami::createCircle(aliceId, R"({"allow":["read-channel","text"],"deny":[]})");
    CPPUNIT_ASSERT(!circle.empty());
    CPPUNIT_ASSERT(wait([&](Events& e) { return e.ready.contains(aliceId + circle); }));
    CPPUNIT_ASSERT(libjami::getCircles(aliceId) == std::vector<std::string> {circle});

    const std::string avatar(256 * 1024, 'A');
    CPPUNIT_ASSERT(libjami::updateCircleProfile(aliceId,
                                                circle,
                                                {{"title", "Studio"}, {"description", "Design"}, {"avatar", avatar}}));
    CPPUNIT_ASSERT(libjami::inviteCircleMember(aliceId, circle, bobUri));
    CPPUNIT_ASSERT(wait([&](Events& e) { return e.requests.contains(bobId + circle); }));
    const auto requests = libjami::getCircleRequests(bobId);
    CPPUNIT_ASSERT(requests.size() == 1 && requests.front().at("id") == circle);
    CPPUNIT_ASSERT_EQUAL(std::string("Studio"), requests.front().at("name"));
    libjami::acceptCircleRequest(bobId, circle);
    CPPUNIT_ASSERT(wait([&](Events& e) { return e.ready.contains(bobId + circle); }));
    CPPUNIT_ASSERT(waitUntil([&] {
        for (const auto& member : libjami::getCircleMembers(aliceId, circle)) {
            if (member.at("uri") == bobUri)
                return member.at("state") == "active";
        }
        return false;
    }));
    CPPUNIT_ASSERT(libjami::getCirclePolicy(aliceId, circle).find(bobUri) != std::string::npos);
    CPPUNIT_ASSERT_EQUAL(std::string("Studio"), libjami::getCircleProfile(bobId, circle).at("title"));
    CPPUNIT_ASSERT(libjami::getCircleProfile(bobId, circle).at("avatar") == avatar);
    CPPUNIT_ASSERT(!libjami::updateCircleProfile(bobId, circle, {{"title", "Not authorized"}}));
    CPPUNIT_ASSERT(libjami::updateCircleProfile(aliceId, circle, {{"title", "Design studio"}}));
    CPPUNIT_ASSERT(waitUntil([&] { return libjami::getCircleProfile(bobId, circle)["title"] == "Design studio"; }));
    CPPUNIT_ASSERT_EQUAL(std::string("Design"), libjami::getCircleProfile(bobId, circle).at("description"));
    CPPUNIT_ASSERT(libjami::updateCirclePolicy(aliceId,
                                               circle,
                                               R"({"type":"set-role","role":"writer",)"
                                               R"("rule":{"allow":["text"],"deny":[]}})"));
    // Lifecycle operations are not policy updates.
    CPPUNIT_ASSERT(!libjami::updateCirclePolicy(aliceId, circle, R"({"type":"end-circle"})"));

    const auto channel = libjami::createCircleChannel(aliceId, circle);
    CPPUNIT_ASSERT(!channel.empty());
    CPPUNIT_ASSERT(wait([&](Events& e) { return e.channels.contains(aliceId + channel); }));
    CPPUNIT_ASSERT(waitUntil([&] { return libjami::getCircleChannels(bobId, circle).size() == 1; }));
    CPPUNIT_ASSERT(libjami::updateCircleProfile(aliceId, circle, {{"title", "general"}}, channel));
    CPPUNIT_ASSERT(waitUntil([&] { return libjami::getCircleProfile(bobId, circle, channel)["title"] == "general"; }));
    CPPUNIT_ASSERT_EQUAL(std::string("general"), libjami::getCircleChannels(bobId, circle).front().at("title"));
    libjami::joinCircleChannel(bobId, circle, channel);
    CPPUNIT_ASSERT(wait([&](Events& e) { return e.channels.contains(bobId + channel); }));
    CPPUNIT_ASSERT(libjami::getCircleChannels(bobId, circle).front().at("joined") == "true");

    // A joined channel is written and read with the conversation API, without being a conversation.
    const auto conversations = libjami::getConversations(bobId);
    CPPUNIT_ASSERT(std::find(conversations.begin(), conversations.end(), channel) == conversations.end());
    libjami::sendMessage(aliceId, channel, "hello", "");
    CPPUNIT_ASSERT(wait([&](Events& e) {
        return std::any_of(e.bobMessages.begin(), e.bobMessages.end(), [&](const auto& message) {
            const auto body = message.body.find("body");
            return body != message.body.end() && body->second == "hello";
        });
    }));
    // What a client was signalled is not loaded again.
    libjami::clearCache(bobId, channel);
    const auto id = libjami::loadConversation(bobId, channel, "", 10);
    CPPUNIT_ASSERT(id);
    CPPUNIT_ASSERT(wait([&](Events& e) { return e.loadId == id; }));
    CPPUNIT_ASSERT(events->loaded.size() == 2 && events->loaded.front().body.at("body") == "hello");
    CPPUNIT_ASSERT(events->loaded.back().id == channel && events->loaded.back().type == "initial");
    CPPUNIT_ASSERT((events->permissions == std::vector<std::string> {"text", "readChannel"}));
    CPPUNIT_ASSERT(libjami::getConversationPermissions(bobId, channel) == events->permissions);
    // The circle lets members write, not react.
    libjami::sendMessage(bobId, channel, "+1", events->loaded.front().id, 2);
    CPPUNIT_ASSERT(wait([&](Events& e) { return e.errors.contains(bobId + channel); }));

    const auto restricted = libjami::createCircleChannel(aliceId,
                                                         circle,
                                                         R"({"defaults":{"allow":[],"deny":[]},"roles":{}})");
    CPPUNIT_ASSERT(!restricted.empty());
    CPPUNIT_ASSERT(libjami::updateCircleProfile(aliceId, circle, {{"title", "restricted"}}, restricted));
    CPPUNIT_ASSERT(
        waitUntil([&] { return libjami::getCircleProfile(bobId, circle, restricted)["title"] == "restricted"; }));
    CPPUNIT_ASSERT(!libjami::updateCircleProfile(bobId, circle, {{"title", "unauthorized"}}, restricted));
    CPPUNIT_ASSERT(!libjami::updateCircleProfile(aliceId, circle, {{"avatar", avatar}}, restricted));
    CPPUNIT_ASSERT(libjami::getConversationPermissions(bobId, restricted).empty());

    const auto another = libjami::createCircle(aliceId);
    const auto longName = std::string(255, 'a') + "\xc3\xa9";
    CPPUNIT_ASSERT(libjami::updateCircleProfile(aliceId, another, {{"title", longName}}));
    CPPUNIT_ASSERT(libjami::inviteCircleMember(aliceId, another, bobUri));
    CPPUNIT_ASSERT(waitUntil([&] {
        for (const auto& request : libjami::getCircleRequests(bobId)) {
            if (request.at("id") == another)
                return request.at("name").empty();
        }
        return false;
    }));
    CPPUNIT_ASSERT_EQUAL(longName, libjami::getCircleProfile(aliceId, another).at("title"));

    CPPUNIT_ASSERT(libjami::removeCircle(bobId, circle));
    CPPUNIT_ASSERT(wait([&](Events& e) { return e.removed.contains(bobId + circle); }));
    CPPUNIT_ASSERT(libjami::getCircles(bobId).empty());
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::CircleSyncTest::name())
