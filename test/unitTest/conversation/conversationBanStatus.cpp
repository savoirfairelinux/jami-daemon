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
#include <chrono>
#include <filesystem>
#include <functional>
#include <thread>

#include "manager.h"
#include "fileutils.h"
#include "jami.h"
#include "conversation/conversationcommon.h"
#include "common.h"
#include "../../test_runner.h"

using namespace std::literals::chrono_literals;

namespace jami {
namespace test {

class ConversationBanStatusTest : public CppUnit::TestFixture
{
public:
    ~ConversationBanStatusTest() { libjami::fini(); }
    static std::string name() { return "ConversationBanStatusTest"; }

    void setUp();
    void tearDown();

private:
    void testSplitMemberAndDeviceBanStatus();
    void testPeerAuthorizationChecksMembershipAndInvites();

    bool waitFor(const std::function<bool()>& predicate, std::chrono::seconds timeout = 30s) const;
    bool isConnectedToDevice(const std::shared_ptr<Conversation>& conv, const std::string& deviceId) const;
    std::filesystem::path buildRoot() const;
    std::filesystem::path conversationPath(const std::string& accountId, const std::string& convId) const;

    std::string aliceId;
    std::string bobId;
    std::string carlaId;

    CPPUNIT_TEST_SUITE(ConversationBanStatusTest);
    CPPUNIT_TEST(testSplitMemberAndDeviceBanStatus);
    CPPUNIT_TEST(testPeerAuthorizationChecksMembershipAndInvites);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ConversationBanStatusTest, ConversationBanStatusTest::name());

void
ConversationBanStatusTest::setUp()
{
    const auto root = buildRoot();

    libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
    if (not Manager::instance().initialized)
        CPPUNIT_ASSERT(libjami::start((root / "jami-sample.yml").string()));

    auto actors = load_actors(root / "actors" / "alice-bob-carla.yml");
    aliceId = actors["alice"];
    bobId = actors["bob"];
    carlaId = actors["carla"];

    Manager::instance().sendRegister(carlaId, false);
    wait_for_announcement_of({aliceId, bobId});
}

void
ConversationBanStatusTest::tearDown()
{
    wait_for_removal_of({aliceId, bobId, carlaId});
}

bool
ConversationBanStatusTest::waitFor(const std::function<bool()>& predicate, std::chrono::seconds timeout) const
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate())
            return true;
        std::this_thread::sleep_for(100ms);
    }
    return predicate();
}

bool
ConversationBanStatusTest::isConnectedToDevice(const std::shared_ptr<Conversation>& conv,
                                               const std::string& deviceId) const
{
    for (const auto& node : conv->peersToSyncWith())
        if (node.toString() == deviceId)
            return true;
    return false;
}

std::filesystem::path
ConversationBanStatusTest::buildRoot() const
{
    auto root = std::filesystem::current_path();
    if (std::filesystem::is_regular_file(root / "jami-sample.yml") && std::filesystem::is_directory(root / "actors")) {
        return root;
    }
    if (std::filesystem::is_regular_file(root.parent_path() / "jami-sample.yml")
        && std::filesystem::is_directory(root.parent_path() / "actors")) {
        return root.parent_path();
    }
    return root;
}

std::filesystem::path
ConversationBanStatusTest::conversationPath(const std::string& accountId, const std::string& convId) const
{
    return fileutils::get_data_dir() / accountId / "conversations" / convId;
}

void
ConversationBanStatusTest::testSplitMemberAndDeviceBanStatus()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto bobDeviceId = std::string(bobAccount->currentDeviceId());

    auto deviceConvId = libjami::startConversation(aliceId);
    libjami::addConversationMember(aliceId, deviceConvId, bobUri);
    CPPUNIT_ASSERT(waitFor([&] { return !libjami::getConversationRequests(bobId).empty(); }));

    libjami::acceptConversationRequest(bobId, deviceConvId);
    const auto bobDeviceFile = conversationPath(aliceId, deviceConvId) / "devices" / (bobDeviceId + ".crt");
    CPPUNIT_ASSERT(waitFor([&] { return std::filesystem::is_regular_file(bobDeviceFile); }));

    auto deviceConv = aliceAccount->convModule()->getConversation(deviceConvId);
    CPPUNIT_ASSERT(deviceConv != nullptr);
    CPPUNIT_ASSERT(!deviceConv->isMemberBanned(bobUri));
    CPPUNIT_ASSERT(!deviceConv->isDeviceBanned(bobDeviceId));

    // Bob's device must be connected in the swarm before we ban it.
    CPPUNIT_ASSERT(waitFor([&] { return isConnectedToDevice(deviceConv, bobDeviceId); }));

    std::atomic_bool deviceBanDone {false};
    deviceConv->removeMember(bobDeviceId, true, [&](bool ok, const std::string&) { deviceBanDone = ok; });
    CPPUNIT_ASSERT(waitFor([&] { return deviceBanDone.load(); }));
    CPPUNIT_ASSERT(waitFor([&] { return deviceConv->isDeviceBanned(bobDeviceId); }));
    CPPUNIT_ASSERT(!deviceConv->isMemberBanned(bobUri));
    // Banning the device must disconnect it from the swarm.
    CPPUNIT_ASSERT(waitFor([&] { return !isConnectedToDevice(deviceConv, bobDeviceId); }));

    Manager::instance().sendRegister(carlaId, true);
    wait_for_announcement_of(carlaId);

    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    auto carlaDeviceId = std::string(carlaAccount->currentDeviceId());

    auto memberConvId = libjami::startConversation(aliceId);
    libjami::addConversationMember(aliceId, memberConvId, carlaUri);
    CPPUNIT_ASSERT(waitFor([&] { return !libjami::getConversationRequests(carlaId).empty(); }));

    libjami::acceptConversationRequest(carlaId, memberConvId);
    const auto carlaDeviceFile = conversationPath(aliceId, memberConvId) / "devices" / (carlaDeviceId + ".crt");
    CPPUNIT_ASSERT(waitFor([&] { return std::filesystem::is_regular_file(carlaDeviceFile); }));

    auto memberConv = aliceAccount->convModule()->getConversation(memberConvId);
    CPPUNIT_ASSERT(memberConv != nullptr);
    CPPUNIT_ASSERT(!memberConv->isMemberBanned(carlaUri));
    CPPUNIT_ASSERT(!memberConv->isDeviceBanned(carlaDeviceId));

    // Carla's device must be connected in the swarm before we ban the member.
    CPPUNIT_ASSERT(waitFor([&] { return isConnectedToDevice(memberConv, carlaDeviceId); }));

    std::atomic_bool memberBanDone {false};
    memberConv->removeMember(carlaUri, false, [&](bool ok, const std::string&) { memberBanDone = ok; });
    CPPUNIT_ASSERT(waitFor([&] { return memberBanDone.load(); }));
    CPPUNIT_ASSERT(waitFor([&] { return memberConv->isMemberBanned(carlaUri); }));
    CPPUNIT_ASSERT(!memberConv->isDeviceBanned(carlaDeviceId));
    // Banning the member must disconnect all of their devices from the swarm.
    CPPUNIT_ASSERT(waitFor([&] { return !isConnectedToDevice(memberConv, carlaDeviceId); }));
}

void
ConversationBanStatusTest::testPeerAuthorizationChecksMembershipAndInvites()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);

    auto bobUri = bobAccount->getUsername();
    auto bobDeviceId = std::string(bobAccount->currentDeviceId());
    auto carlaUri = carlaAccount->getUsername();
    auto carlaDeviceId = std::string(carlaAccount->currentDeviceId());

    auto memberConvId = libjami::startConversation(aliceId);
    libjami::addConversationMember(aliceId, memberConvId, bobUri);
    CPPUNIT_ASSERT(waitFor([&] { return !libjami::getConversationRequests(bobId).empty(); }));

    libjami::acceptConversationRequest(bobId, memberConvId);
    const auto bobDeviceFile = conversationPath(aliceId, memberConvId) / "devices" / (bobDeviceId + ".crt");
    CPPUNIT_ASSERT(waitFor([&] { return std::filesystem::is_regular_file(bobDeviceFile); }));

    auto* convModule = aliceAccount->convModule();
    CPPUNIT_ASSERT(convModule != nullptr);
    CPPUNIT_ASSERT(convModule->isPeerAuthorized(memberConvId, bobUri, bobDeviceId, true));
    CPPUNIT_ASSERT(!convModule->isPeerAuthorized(memberConvId, carlaUri, carlaDeviceId, true));

    auto inviteConvId = libjami::startConversation(aliceId);
    libjami::addConversationMember(aliceId, inviteConvId, carlaUri);

    const auto invitedFile = conversationPath(aliceId, inviteConvId) / "invited" / carlaUri;
    CPPUNIT_ASSERT(waitFor([&] { return std::filesystem::is_regular_file(invitedFile); }));

    CPPUNIT_ASSERT(convModule->isPeerAuthorized(inviteConvId, carlaUri, carlaDeviceId, true));
    CPPUNIT_ASSERT(!convModule->isPeerAuthorized(inviteConvId, carlaUri, carlaDeviceId, false));
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::ConversationBanStatusTest::name())