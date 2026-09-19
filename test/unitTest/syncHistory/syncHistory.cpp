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

#include "fileutils.h"
#include "manager.h"
#include "jamidht/jamiaccount.h"
#include "jamidht/conversation_module.h"
#include "jamidht/sync_module.h"
#include "../../test_runner.h"
#include "jami.h"
#include "account_const.h"
#include "common.h"

#include <dhtnet/connectionmanager.h>
#include <dhtnet/multiplexed_socket.h>

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <thread>

using namespace libjami::Account;
using namespace std::literals::chrono_literals;

namespace jami {
namespace test {

struct LegacySyncConvInfo
{
    std::string id;
    time_t created {};
    time_t removed {};
    time_t erased {};
    std::set<std::string> members;
    std::string lastDisplayed;
    ConversationMode mode {};
    MSGPACK_DEFINE_MAP(id, created, removed, erased, members, lastDisplayed, mode)
};

struct LegacySyncMessage
{
    DeviceSync ds;
    std::map<std::string, LegacySyncConvInfo> c;
    decltype(SyncMsg::cr) cr;
    decltype(SyncMsg::p) p;
    decltype(SyncMsg::ld) ld;
    decltype(SyncMsg::ms) ms;
    MSGPACK_DEFINE(ds, c, cr, p, ld, ms)
};

static SyncMsg
legacySnapshot(const ConvInfo& info)
{
    LegacySyncMessage message;
    message.c.emplace(info.id,
                      LegacySyncConvInfo {info.id,
                                          toSecondsSinceEpoch(info.created),
                                          toSecondsSinceEpoch(info.removed),
                                          toSecondsSinceEpoch(info.erased),
                                          info.members,
                                          info.lastDisplayed,
                                          info.mode});
    msgpack::sbuffer buffer;
    msgpack::pack(buffer, message);
    return msgpack::unpack(buffer.data(), buffer.size()).get().as<SyncMsg>();
}

struct UserData
{
    std::string conversationId;
    bool removed {false};
    bool requestReceived {false};
    bool requestRemoved {false};
    bool errorDetected {false};
    bool registered {false};
    bool stopped {false};
    bool deviceAnnounced {false};
    bool sending {false};
    bool sent {false};
    std::string profilePath;
    std::string payloadTrustRequest;
    std::vector<libjami::SwarmMessage> messages;
    std::vector<libjami::SwarmMessage> messagesLoaded;
    std::vector<libjami::SwarmMessage> messagesUpdated;
    std::map<std::string, int> members;
};

class SyncHistoryTest : public CppUnit::TestFixture
{
public:
    SyncHistoryTest()
    {
        // Init daemon
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    }
    ~SyncHistoryTest() { libjami::fini(); }
    static std::string name() { return "SyncHistory"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    UserData aliceData;
    std::string bobId;
    UserData bobData;
    std::string alice2Id;
    UserData alice2Data;

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    void connectSignals();

private:
    void testCreateConversationThenSync();
    void testCreateConversationWithOnlineDevice();
    void testCreateConversationWithMessagesThenAddDevice();
    void testCreateMultipleConversationThenAddDevice();
    void testReceivesInviteThenAddDevice();
    void testRemoveConversationOnAllDevices();
    void testSyncedRemovalPreservesTimestamp();
    void testSyncedErasureAfterRemoval();
    void testSyncedRemovalOrdering();
    void testLegacyRemovalAcceptsLaterErasure();
    void testLegacyRemovalAcceptsEarlierErasure();
    void checkLegacyRemovalErasure(std::chrono::milliseconds erasureDelay);
    void testOlderGenerationErasureIsIgnored();
    ConvInfo createPreciseConversation();
    void testLegacySecondsRemoval();
    void testLegacySecondsErasureAfterReceipt();
    void testLegacyRelayKeepsPreciseCreation();
    void testLegacySecondsCannotEraseRejoin();
    void testSyncCreateAccountExportDeleteReimportOldBackup();
    void testSyncCreateAccountExportDeleteReimportWithConvId();
    void testSyncCreateAccountExportDeleteReimportWithConvReq();
    void testSyncOneToOne();
    void testConversationRequestRemoved();
    void testProfileReceivedMultiDevice();
    void testLastInteractionAfterClone();
    void testLastInteractionAfterSomeMessages();

    CPPUNIT_TEST_SUITE(SyncHistoryTest);
    CPPUNIT_TEST(testCreateConversationThenSync);
    CPPUNIT_TEST(testCreateConversationWithOnlineDevice);
    CPPUNIT_TEST(testCreateConversationWithMessagesThenAddDevice);
    CPPUNIT_TEST(testCreateMultipleConversationThenAddDevice);
    CPPUNIT_TEST(testReceivesInviteThenAddDevice);
    CPPUNIT_TEST(testRemoveConversationOnAllDevices);
    CPPUNIT_TEST(testSyncedRemovalPreservesTimestamp);
    CPPUNIT_TEST(testSyncedErasureAfterRemoval);
    CPPUNIT_TEST(testSyncedRemovalOrdering);
    CPPUNIT_TEST(testLegacyRemovalAcceptsLaterErasure);
    CPPUNIT_TEST(testLegacyRemovalAcceptsEarlierErasure);
    CPPUNIT_TEST(testOlderGenerationErasureIsIgnored);
    CPPUNIT_TEST(testLegacySecondsRemoval);
    CPPUNIT_TEST(testLegacySecondsErasureAfterReceipt);
    CPPUNIT_TEST(testLegacyRelayKeepsPreciseCreation);
    CPPUNIT_TEST(testLegacySecondsCannotEraseRejoin);
    CPPUNIT_TEST(testSyncCreateAccountExportDeleteReimportOldBackup);
    CPPUNIT_TEST(testSyncCreateAccountExportDeleteReimportWithConvId);
    CPPUNIT_TEST(testSyncCreateAccountExportDeleteReimportWithConvReq);
    CPPUNIT_TEST(testSyncOneToOne);
    CPPUNIT_TEST(testConversationRequestRemoved);
    CPPUNIT_TEST(testProfileReceivedMultiDevice);
    CPPUNIT_TEST(testLastInteractionAfterClone);
    CPPUNIT_TEST(testLastInteractionAfterSomeMessages);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(SyncHistoryTest, SyncHistoryTest::name());

void
SyncHistoryTest::testSyncedRemovalPreservesTimestamp()
{
    auto account = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto convId = libjami::startConversation(aliceId);
    auto removed = ConversationModule::convInfos(aliceId).at(convId);
    removed.removed = removed.created + 1ms;
    std::this_thread::sleep_until(removed.removed + 5ms);
    SyncMsg msg;
    msg.c[convId] = removed;
    account->convModule()->onSyncData(msg, account->getUsername(), "");
    auto stored = ConversationModule::convInfos(aliceId).at(convId);
    CPPUNIT_ASSERT_EQUAL(toMillisecondsSinceEpoch(removed.removed), toMillisecondsSinceEpoch(stored.removed));

    account->convModule()->onSyncData(msg, account->getUsername(), "");
    stored = ConversationModule::convInfos(aliceId).at(convId);
    CPPUNIT_ASSERT_EQUAL(toMillisecondsSinceEpoch(removed.removed), toMillisecondsSinceEpoch(stored.removed));
}

void
SyncHistoryTest::testSyncedErasureAfterRemoval()
{
    auto account = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto convId = libjami::startConversation(aliceId);
    auto removed = ConversationModule::convInfos(aliceId).at(convId);
    removed.removed = removed.created + 1ms;
    std::this_thread::sleep_until(removed.removed + 5ms);
    SyncMsg msg;
    msg.c[convId] = removed;
    account->convModule()->onSyncData(msg, account->getUsername(), "");
    const auto repo = fileutils::get_data_dir() / aliceId / "conversations" / convId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repo));

    // The originating device confirms erasure after a peer fetched its leave.
    msg.c[convId].erased = removed.removed + 2ms;
    account->convModule()->onSyncData(msg, account->getUsername(), "");
    CPPUNIT_ASSERT(!std::filesystem::exists(repo));
    auto stored = ConversationModule::convInfos(aliceId).at(convId);
    CPPUNIT_ASSERT_EQUAL(toMillisecondsSinceEpoch(removed.removed), toMillisecondsSinceEpoch(stored.removed));
    CPPUNIT_ASSERT_EQUAL(toMillisecondsSinceEpoch(msg.c.at(convId).erased), toMillisecondsSinceEpoch(stored.erased));

    // A delayed removal-only snapshot must not undo the erasure.
    msg.c[convId].erased = {};
    account->convModule()->onSyncData(msg, account->getUsername(), "");
    CPPUNIT_ASSERT(ConversationModule::convInfos(aliceId).at(convId).erased == stored.erased);
}

void
SyncHistoryTest::testSyncedRemovalOrdering()
{
    auto account = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto convId = libjami::startConversation(aliceId);
    auto original = ConversationModule::convInfos(aliceId).at(convId);
    SyncMsg msg;
    msg.c[convId] = original;
    msg.c[convId].removed = original.created + 2ms;
    std::this_thread::sleep_until(original.created + 10ms);
    account->convModule()->onSyncData(msg, account->getUsername(), "");
    msg.c[convId].removed += 1ms;
    account->convModule()->onSyncData(msg, account->getUsername(), "");
    auto stored = ConversationModule::convInfos(aliceId).at(convId);
    CPPUNIT_ASSERT(stored.removed == msg.c.at(convId).removed);

    msg.c[convId].removed -= 1ms;
    account->convModule()->onSyncData(msg, account->getUsername(), "");
    CPPUNIT_ASSERT(ConversationModule::convInfos(aliceId).at(convId).removed == stored.removed);
    msg.c[convId] = original;
    account->convModule()->onSyncData(msg, account->getUsername(), "");
    CPPUNIT_ASSERT(ConversationModule::convInfos(aliceId).at(convId).removed == stored.removed);

    msg.c[convId].removed = stored.removed;
    msg.c[convId].erased = stored.removed + 1ms;
    account->convModule()->onSyncData(msg, account->getUsername(), "");
    const auto staleRemoval = msg.c.at(convId);

    auto rejoined = original;
    rejoined.created = staleRemoval.erased + 1ms;
    msg.c[convId] = rejoined;
    account->convModule()->onSyncData(msg, account->getUsername(), "");
    CPPUNIT_ASSERT(!ConversationModule::convInfos(aliceId).at(convId).isRemoved());

    msg.c[convId] = staleRemoval;
    account->convModule()->onSyncData(msg, account->getUsername(), "");
    stored = ConversationModule::convInfos(aliceId).at(convId);
    CPPUNIT_ASSERT(stored.created == rejoined.created);
    CPPUNIT_ASSERT(!stored.isRemoved());
}

void
SyncHistoryTest::testLegacyRemovalAcceptsLaterErasure()
{
    checkLegacyRemovalErasure(40ms);
}

void
SyncHistoryTest::testLegacyRemovalAcceptsEarlierErasure()
{
    checkLegacyRemovalErasure(20ms);
}

void
SyncHistoryTest::checkLegacyRemovalErasure(std::chrono::milliseconds erasureDelay)
{
    auto account = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto convId = libjami::startConversation(aliceId);
    auto original = ConversationModule::convInfos(aliceId).at(convId);
    SyncMsg msg;
    msg.c[convId] = original;
    // Seed the receipt-time removal stored by an older daemon.
    msg.c[convId].removed = original.created + 30ms;
    account->convModule()->onSyncData(msg, account->getUsername(), "");
    auto legacy = ConversationModule::convInfos(aliceId).at(convId);
    CPPUNIT_ASSERT(legacy.removed == msg.c.at(convId).removed);
    const auto repo = fileutils::get_data_dir() / aliceId / "conversations" / convId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repo));

    // The original removal predates local receipt. Erasure may do so too.
    msg.c[convId].removed = original.created + 10ms;
    msg.c[convId].erased = original.created + erasureDelay;
    account->convModule()->onSyncData(msg, account->getUsername(), "");
    CPPUNIT_ASSERT(!std::filesystem::exists(repo));
    auto stored = ConversationModule::convInfos(aliceId).at(convId);
    CPPUNIT_ASSERT(stored.erased == msg.c.at(convId).erased);
    CPPUNIT_ASSERT(stored.removed == legacy.removed);
    account->convModule()->onSyncData(msg, account->getUsername(), "");
    CPPUNIT_ASSERT(ConversationModule::convInfos(aliceId).at(convId).erased == stored.erased);
}

void
SyncHistoryTest::testOlderGenerationErasureIsIgnored()
{
    auto account = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto convId = libjami::startConversation(aliceId);
    const auto current = ConversationModule::convInfos(aliceId).at(convId);
    SyncMsg msg;
    msg.c[convId] = current;
    msg.c[convId].created -= 20ms;
    msg.c[convId].removed = current.created - 10ms;
    msg.c[convId].erased = current.created + 100ms;
    account->convModule()->onSyncData(msg, account->getUsername(), "");
    auto stored = ConversationModule::convInfos(aliceId).at(convId);
    CPPUNIT_ASSERT(!stored.isRemoved());
    CPPUNIT_ASSERT(stored.created == current.created);
    CPPUNIT_ASSERT(stored.erased == TimePoint {});
    CPPUNIT_ASSERT(std::filesystem::is_directory(fileutils::get_data_dir() / aliceId / "conversations" / convId));

    // A legacy receipt timestamp can even be later than the current rejoin.
    // The older creation generation must still prevent deleting this replica.
    msg.c[convId].removed = current.created + 50ms;
    account->convModule()->onSyncData(msg, account->getUsername(), "");
    stored = ConversationModule::convInfos(aliceId).at(convId);
    CPPUNIT_ASSERT(!stored.isRemoved());
    CPPUNIT_ASSERT(stored.created == current.created);
    CPPUNIT_ASSERT(std::filesystem::is_directory(fileutils::get_data_dir() / aliceId / "conversations" / convId));
}

void
SyncHistoryTest::testLegacySecondsRemoval()
{
    auto account = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto info = createPreciseConversation();
    info.removed = timePointFromMilliseconds(1700000002345);
    account->convModule()->onSyncData(legacySnapshot(info), account->getUsername(), "");
    auto stored = ConversationModule::convInfos(aliceId).at(info.id);
    CPPUNIT_ASSERT(stored.isRemoved());
    CPPUNIT_ASSERT(stored.created == info.created);
    info.erased = timePointFromMilliseconds(1700000003456);
    account->convModule()->onSyncData(legacySnapshot(info), account->getUsername(), "");
    CPPUNIT_ASSERT(!std::filesystem::exists(fileutils::get_data_dir() / aliceId / "conversations" / info.id));
}

void
SyncHistoryTest::testLegacySecondsErasureAfterReceipt()
{
    auto account = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto info = createPreciseConversation();
    auto receipt = info;
    receipt.removed = timePointFromMilliseconds(1700000005789);
    SyncMsg msg;
    msg.c[info.id] = receipt;
    account->convModule()->onSyncData(msg, account->getUsername(), "");
    CPPUNIT_ASSERT(ConversationModule::convInfos(aliceId).at(info.id).removed == receipt.removed);
    info.removed = timePointFromMilliseconds(1700000002345);
    info.erased = timePointFromMilliseconds(1700000003456);
    auto legacy = legacySnapshot(info);
    // Also exercise a modern daemon forwarding an older peer's payload.
    msgpack::sbuffer buffer;
    msgpack::pack(buffer, legacy);
    auto forwarded = msgpack::unpack(buffer.data(), buffer.size()).get().as<SyncMsg>();
    account->convModule()->onSyncData(forwarded, account->getUsername(), "");
    CPPUNIT_ASSERT(!std::filesystem::exists(fileutils::get_data_dir() / aliceId / "conversations" / info.id));
}

void
SyncHistoryTest::testLegacyRelayKeepsPreciseCreation()
{
    auto account = Manager::instance().getAccount<JamiAccount>(aliceId);
    const auto info = createPreciseConversation();
    account->convModule()->onSyncData(legacySnapshot(info), account->getUsername(), "");
    auto older = info;
    older.created -= 50ms;
    older.removed = timePointFromMilliseconds(1700000003000);
    older.erased = older.removed + 1s;
    SyncMsg msg;
    msg.c[info.id] = older;
    account->convModule()->onSyncData(msg, account->getUsername(), "");
    CPPUNIT_ASSERT(!ConversationModule::convInfos(aliceId).at(info.id).isRemoved());
    CPPUNIT_ASSERT(std::filesystem::is_directory(fileutils::get_data_dir() / aliceId / "conversations" / info.id));
}

void
SyncHistoryTest::testLegacySecondsCannotEraseRejoin()
{
    auto account = Manager::instance().getAccount<JamiAccount>(aliceId);
    const auto info = createPreciseConversation();
    auto older = info;
    older.created -= 50ms;
    older.removed = info.created - 1ms;
    older.erased = info.created + 1s;
    account->convModule()->onSyncData(legacySnapshot(older), account->getUsername(), "");
    CPPUNIT_ASSERT(!ConversationModule::convInfos(aliceId).at(info.id).isRemoved());
    CPPUNIT_ASSERT(std::filesystem::is_directory(fileutils::get_data_dir() / aliceId / "conversations" / info.id));
    older.created -= 1s;
    older.removed = info.created + 2s;
    older.erased = info.created + 3s;
    account->convModule()->onSyncData(legacySnapshot(older), account->getUsername(), "");
    CPPUNIT_ASSERT(!ConversationModule::convInfos(aliceId).at(info.id).isRemoved());
}

ConvInfo
SyncHistoryTest::createPreciseConversation()
{
    auto convId = libjami::startConversation(aliceId);
    auto infos = ConversationModule::convInfos(aliceId);
    infos.at(convId).created = timePointFromMilliseconds(1700000001123);
    ConversationModule::saveConvInfos(aliceId, infos);
    Manager::instance().getAccount<JamiAccount>(aliceId)->convModule()->loadConversations();
    return infos.at(convId);
}

void
SyncHistoryTest::setUp()
{
    auto actors = load_actors_and_wait_for_announcement("actors/alice-bob.yml");
    aliceId = actors["alice"];
    bobId = actors["bob"];
    alice2Id = "";
    aliceData = {};
    bobData = {};
    alice2Data = {};
}

void
SyncHistoryTest::tearDown()
{
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
    std::remove(aliceArchive.c_str());
    if (alice2Id.empty()) {
        wait_for_removal_of({aliceId, bobId});
    } else {
        wait_for_removal_of({aliceId, bobId, alice2Id});
    }
}

void
SyncHistoryTest::connectSignals()
{
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
        [&](const std::string& accountId, const std::map<std::string, std::string>&) {
            if (accountId == aliceId) {
                auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
                auto details = aliceAccount->getVolatileAccountDetails();
                auto daemonStatus = details[libjami::Account::ConfProperties::Registration::STATUS];
                if (daemonStatus == "REGISTERED") {
                    aliceData.registered = true;
                } else if (daemonStatus == "UNREGISTERED") {
                    aliceData.stopped = true;
                }
                auto deviceAnnounced = details[libjami::Account::VolatileProperties::DEVICE_ANNOUNCED];
                aliceData.deviceAnnounced = deviceAnnounced == "true";
            } else if (accountId == bobId) {
                auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
                auto details = bobAccount->getVolatileAccountDetails();
                auto daemonStatus = details[libjami::Account::ConfProperties::Registration::STATUS];
                if (daemonStatus == "REGISTERED") {
                    bobData.registered = true;
                } else if (daemonStatus == "UNREGISTERED") {
                    bobData.stopped = true;
                }
                auto deviceAnnounced = details[libjami::Account::VolatileProperties::DEVICE_ANNOUNCED];
                bobData.deviceAnnounced = deviceAnnounced == "true";
            } else if (accountId == alice2Id) {
                auto alice2Account = Manager::instance().getAccount<JamiAccount>(alice2Id);
                auto details = alice2Account->getVolatileAccountDetails();
                auto daemonStatus = details[libjami::Account::ConfProperties::Registration::STATUS];
                if (daemonStatus == "REGISTERED") {
                    alice2Data.registered = true;
                } else if (daemonStatus == "UNREGISTERED") {
                    alice2Data.stopped = true;
                }
                auto deviceAnnounced = details[libjami::Account::VolatileProperties::DEVICE_ANNOUNCED];
                alice2Data.deviceAnnounced = deviceAnnounced == "true";
            }
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == aliceId) {
                aliceData.conversationId = conversationId;
            } else if (accountId == bobId) {
                bobData.conversationId = conversationId;
            } else if (accountId == alice2Id) {
                alice2Data.conversationId = conversationId;
            }
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::ProfileReceived>(
        [&](const std::string& accountId, const std::string& /*peerId*/, const std::string& path) {
            if (accountId == bobId)
                bobData.profilePath = path;
            else if (accountId == aliceId)
                aliceData.profilePath = path;
            else if (accountId == alice2Id)
                alice2Data.profilePath = path;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationMemberEvent>(
        [&](const std::string& accountId, const std::string& /*conversationId*/, const std::string& member, int status) {
            if (accountId == aliceId) {
                aliceData.members[member] = status;
            } else if (accountId == bobId) {
                bobData.members[member] = status;
            } else if (accountId == alice2Id) {
                alice2Data.members[member] = status;
            }
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::AccountMessageStatusChanged>(
        [&](const std::string& accountId,
            const std::string& /*conversationId*/,
            const std::string& /*peer*/,
            const std::string& /*msgId*/,
            int status) {
            if (accountId == aliceId) {
                if (status == 2)
                    aliceData.sending = true;
                if (status == 3)
                    aliceData.sent = true;
            } else if (accountId == alice2Id) {
                if (status == 2)
                    alice2Data.sending = true;
                if (status == 3)
                    alice2Data.sent = true;
            } else if (accountId == bobId) {
                if (status == 2)
                    bobData.sending = true;
                if (status == 3)
                    bobData.sent = true;
            }
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::IncomingTrustRequest>(
        [&](const std::string& account_id,
            const std::string& /*conversationId*/,
            const std::string& /*from*/,
            const std::vector<uint8_t>& payload,
            time_t /*received*/) {
            auto payloadStr = std::string(payload.data(), payload.data() + payload.size());
            if (account_id == aliceId)
                aliceData.payloadTrustRequest = payloadStr;
            else if (account_id == bobId)
                bobData.payloadTrustRequest = payloadStr;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> /*metadatas*/) {
            if (accountId == aliceId) {
                aliceData.requestReceived = true;
            } else if (accountId == bobId) {
                bobData.requestReceived = true;
            } else if (accountId == alice2Id) {
                alice2Data.requestReceived = true;
            }
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestDeclined>(
        [&](const std::string& accountId, const std::string&) {
            if (accountId == bobId) {
                bobData.requestRemoved = true;
            } else if (accountId == aliceId) {
                aliceData.requestRemoved = true;
            } else if (accountId == alice2Id) {
                alice2Data.requestRemoved = true;
            }
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::SwarmMessageReceived>(
        [&](const std::string& accountId, const std::string& /* conversationId */, libjami::SwarmMessage message) {
            if (accountId == aliceId) {
                aliceData.messages.emplace_back(message);
            } else if (accountId == bobId) {
                bobData.messages.emplace_back(message);
            } else if (accountId == alice2Id) {
                alice2Data.messages.emplace_back(message);
            }
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::SwarmLoaded>(
        [&](uint32_t,
            const std::string& accountId,
            const std::string& /* conversationId */,
            std::vector<libjami::SwarmMessage> messages) {
            if (accountId == aliceId) {
                aliceData.messagesLoaded.insert(aliceData.messagesLoaded.end(), messages.begin(), messages.end());
            } else if (accountId == alice2Id) {
                alice2Data.messagesLoaded.insert(alice2Data.messagesLoaded.end(), messages.begin(), messages.end());
            } else if (accountId == bobId) {
                bobData.messagesLoaded.insert(bobData.messagesLoaded.end(), messages.begin(), messages.end());
            }
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::SwarmMessageUpdated>(
        [&](const std::string& accountId, const std::string& /* conversationId */, libjami::SwarmMessage message) {
            if (accountId == aliceId) {
                aliceData.messagesUpdated.emplace_back(message);
            } else if (accountId == bobId) {
                bobData.messagesUpdated.emplace_back(message);
            }
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::OnConversationError>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            int /*code*/,
            const std::string& /* what */) {
            if (accountId == aliceId)
                aliceData.errorDetected = true;
            else if (accountId == bobId)
                bobData.errorDetected = true;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationRemoved>(
        [&](const std::string& accountId, const std::string&) {
            if (accountId == aliceId)
                aliceData.removed = true;
            else if (accountId == bobId)
                bobData.removed = true;
            else if (accountId == alice2Id)
                alice2Data.removed = true;
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);
}

void
SyncHistoryTest::testCreateConversationThenSync()
{
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    // Start conversation
    auto convId = libjami::startConversation(aliceId);

    // Now create alice2
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
    aliceAccount->exportArchive(aliceArchive);
    std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE2";
    details[ConfProperties::ALIAS] = "ALICE2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PATH] = aliceArchive;

    alice2Id = Manager::instance().addAccount(details);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return !alice2Data.conversationId.empty(); }));
}

void
SyncHistoryTest::testCreateConversationWithOnlineDevice()
{
    connectSignals();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);

    // Now create alice2
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
    aliceAccount->exportArchive(aliceArchive);
    std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE2";
    details[ConfProperties::ALIAS] = "ALICE2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PATH] = aliceArchive;
    auto convId = libjami::startConversation(aliceId);
    alice2Id = Manager::instance().addAccount(details);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return !alice2Data.conversationId.empty(); }));
}

void
SyncHistoryTest::testCreateConversationWithMessagesThenAddDevice()
{
    connectSignals();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto convId = libjami::startConversation(aliceId);

    // Start conversation
    auto aliceMsgSize = aliceData.messages.size();
    libjami::sendMessage(aliceId, convId, std::string("Message 1"), "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&] { return aliceMsgSize + 1 == aliceData.messages.size(); }));
    libjami::sendMessage(aliceId, convId, std::string("Message 2"), "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&] { return aliceMsgSize + 2 == aliceData.messages.size(); }));
    libjami::sendMessage(aliceId, convId, std::string("Message 3"), "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&] { return aliceMsgSize + 3 == aliceData.messages.size(); }));

    // Now create alice2
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
    aliceAccount->exportArchive(aliceArchive);
    std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE2";
    details[ConfProperties::ALIAS] = "ALICE2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PATH] = aliceArchive;
    alice2Id = Manager::instance().addAccount(details);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return !alice2Data.conversationId.empty(); }));

    libjami::loadConversation(alice2Id, convId, "", 0);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return alice2Data.messagesLoaded.size() == 4; }));

    // Check messages
    CPPUNIT_ASSERT(alice2Data.messagesLoaded[0].body[CommitKey::BODY] == "Message 3");
    CPPUNIT_ASSERT(alice2Data.messagesLoaded[1].body[CommitKey::BODY] == "Message 2");
    CPPUNIT_ASSERT(alice2Data.messagesLoaded[2].body[CommitKey::BODY] == "Message 1");
}

void
SyncHistoryTest::testCreateMultipleConversationThenAddDevice()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    // Start conversation
    auto convId = libjami::startConversation(aliceId);
    libjami::sendMessage(aliceId, convId, std::string("Message 1"), "");
    libjami::sendMessage(aliceId, convId, std::string("Message 2"), "");
    libjami::sendMessage(aliceId, convId, std::string("Message 3"), "");
    std::this_thread::sleep_for(1s);
    auto convId2 = libjami::startConversation(aliceId);
    libjami::sendMessage(aliceId, convId2, std::string("Message 1"), "");
    libjami::sendMessage(aliceId, convId2, std::string("Message 2"), "");
    libjami::sendMessage(aliceId, convId2, std::string("Message 3"), "");
    std::this_thread::sleep_for(1s);
    auto convId3 = libjami::startConversation(aliceId);
    libjami::sendMessage(aliceId, convId3, std::string("Message 1"), "");
    libjami::sendMessage(aliceId, convId3, std::string("Message 2"), "");
    libjami::sendMessage(aliceId, convId3, std::string("Message 3"), "");
    std::this_thread::sleep_for(1s);
    auto convId4 = libjami::startConversation(aliceId);
    libjami::sendMessage(aliceId, convId4, std::string("Message 1"), "");
    libjami::sendMessage(aliceId, convId4, std::string("Message 2"), "");
    libjami::sendMessage(aliceId, convId4, std::string("Message 3"), "");

    // Now create alice2
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
    aliceAccount->exportArchive(aliceArchive);
    std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE2";
    details[ConfProperties::ALIAS] = "ALICE2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PATH] = aliceArchive;
    alice2Id = Manager::instance().addAccount(details);

    std::mutex mtx;
    std::unique_lock lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    std::atomic_int conversationReady = 0;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string&) {
            if (accountId == alice2Id) {
                conversationReady += 1;
                cv.notify_one();
            }
        }));
    libjami::registerSignalHandlers(confHandlers);
    confHandlers.clear();

    // Check if conversation is ready
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() { return conversationReady == 4; }));
}

void
SyncHistoryTest::testReceivesInviteThenAddDevice()
{
    connectSignals();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);

    // Export alice
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
    aliceAccount->exportArchive(aliceArchive);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto uri = aliceAccount->getUsername();

    // Start conversation for Alice
    auto convId = libjami::startConversation(bobId);

    libjami::addConversationMember(bobId, convId, uri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return aliceData.requestReceived; }));

    // Now create alice2
    std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE2";
    details[ConfProperties::ALIAS] = "ALICE2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PATH] = aliceArchive;
    alice2Id = Manager::instance().addAccount(details);

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return alice2Data.requestReceived; }));
}

void
SyncHistoryTest::testRemoveConversationOnAllDevices()
{
    connectSignals();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);

    // Now create alice2
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
    aliceAccount->exportArchive(aliceArchive);
    std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE2";
    details[ConfProperties::ALIAS] = "ALICE2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PATH] = aliceArchive;
    auto convId = libjami::startConversation(aliceId);

    alice2Id = Manager::instance().addAccount(details);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] { return !alice2Data.conversationId.empty(); }));
    libjami::removeConversation(aliceId, aliceData.conversationId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return alice2Data.removed; }));
}

void
SyncHistoryTest::testSyncCreateAccountExportDeleteReimportOldBackup()
{
    connectSignals();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    // Backup alice before start conversation, worst scenario for invites
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
    aliceAccount->exportArchive(aliceArchive);

    // Start conversation
    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return aliceMsgSize + 1 == aliceData.messages.size() && !bobData.conversationId.empty();
    }));

    // disable account (same as removed)
    Manager::instance().sendRegister(aliceId, false);
    std::this_thread::sleep_for(5s);

    std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE2";
    details[ConfProperties::ALIAS] = "ALICE2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PATH] = aliceArchive;
    alice2Id = Manager::instance().addAccount(details);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return alice2Data.deviceAnnounced; }));

    // This will trigger a conversation request. Cause alice2 is unable to know of first conversation
    libjami::sendMessage(bobId, convId, std::string("hi"), "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return alice2Data.requestReceived; }));

    libjami::acceptConversationRequest(alice2Id, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return !alice2Data.conversationId.empty(); }));

    auto bobMsgSize = bobData.messages.size();
    libjami::sendMessage(alice2Id, convId, std::string("hi"), "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobMsgSize + 1 == bobData.messages.size(); }));
}

void
SyncHistoryTest::testSyncCreateAccountExportDeleteReimportWithConvId()
{
    connectSignals();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();

    // Start conversation
    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));

    // We need to track presence to know when to sync
    bobAccount->trackBuddyPresence(aliceUri, true);

    // Backup alice after startConversation with member
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
    aliceAccount->exportArchive(aliceArchive);

    // disable account (same as removed)
    Manager::instance().sendRegister(aliceId, false);
    std::this_thread::sleep_for(5s);

    std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE2";
    details[ConfProperties::ALIAS] = "ALICE2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PATH] = aliceArchive;
    alice2Id = Manager::instance().addAccount(details);
    // Should retrieve conversation, no need for action as the convInfos is in the archive
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return !alice2Data.conversationId.empty(); }));

    auto bobMsgSize = bobData.messages.size();
    libjami::sendMessage(alice2Id, convId, std::string("hi"), "");
    cv.wait_for(lk, 30s, [&]() { return bobMsgSize + 1 == bobData.messages.size(); });
}

void
SyncHistoryTest::testSyncCreateAccountExportDeleteReimportWithConvReq()
{
    connectSignals();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceUri = aliceAccount->getUsername();

    // Start conversation
    auto convId = libjami::startConversation(bobId);

    libjami::addConversationMember(bobId, convId, aliceUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceData.requestReceived; }));

    // Backup alice after startConversation with member
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
    aliceAccount->exportArchive(aliceArchive);

    // disable account (same as removed)
    Manager::instance().sendRegister(aliceId, false);
    std::this_thread::sleep_for(5s);

    std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE2";
    details[ConfProperties::ALIAS] = "ALICE2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PATH] = aliceArchive;
    alice2Id = Manager::instance().addAccount(details);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return alice2Data.deviceAnnounced; }));

    // Should get the same request as before.
    auto bobMsgSize = bobData.messages.size();
    libjami::acceptConversationRequest(alice2Id, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobMsgSize + 1 == bobData.messages.size(); }));
}

void
SyncHistoryTest::testSyncOneToOne()
{
    connectSignals();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);

    aliceAccount->addContact(bobAccount->getUsername());
    // Now create alice2
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
    aliceAccount->exportArchive(aliceArchive);
    std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE2";
    details[ConfProperties::ALIAS] = "ALICE2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PATH] = aliceArchive;

    alice2Id = Manager::instance().addAccount(details);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return !alice2Data.conversationId.empty(); }));
}

void
SyncHistoryTest::testConversationRequestRemoved()
{
    connectSignals();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto uri = aliceAccount->getUsername();

    // Export alice
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
    aliceAccount->exportArchive(aliceArchive);

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);

    // Start conversation for Alice
    auto convId = libjami::startConversation(bobId);

    // Check that alice receives the request
    libjami::addConversationMember(bobId, convId, uri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return aliceData.requestReceived; }));

    // Now create alice2
    std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE2";
    details[ConfProperties::ALIAS] = "ALICE2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PATH] = aliceArchive;
    alice2Id = Manager::instance().addAccount(details);

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return aliceData.requestReceived; }));
    // Now decline trust request, this should trigger ConversationRequestDeclined both sides for Alice
    libjami::declineConversationRequest(aliceId, convId);

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return aliceData.requestRemoved && alice2Data.requestRemoved; }));
}

void
SyncHistoryTest::testProfileReceivedMultiDevice()
{
    connectSignals();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();

    // Export alice
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
    aliceAccount->exportArchive(aliceArchive);

    // Set VCards
    std::string vcard = "BEGIN:VCARD\n\
VERSION:2.1\n\
FN:TITLE\n\
DESCRIPTION:DESC\n\
END:VCARD";
    auto alicePath = fileutils::get_data_dir() / aliceId / "profile.vcf";
    auto bobPath = fileutils::get_data_dir() / bobId / "profile.vcf";
    // Save VCard
    auto p = std::filesystem::path(alicePath);
    dhtnet::fileutils::recursive_mkdir(p.parent_path());
    std::ofstream aliceFile(alicePath);
    if (aliceFile.is_open()) {
        aliceFile << vcard;
        aliceFile.close();
    }
    p = std::filesystem::path(bobPath);
    dhtnet::fileutils::recursive_mkdir(p.parent_path());
    std::ofstream bobFile(bobPath);
    if (bobFile.is_open()) {
        bobFile << vcard;
        bobFile.close();
    }

    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return !bobData.profilePath.empty() && !aliceData.profilePath.empty() && !bobData.conversationId.empty();
    }));
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(bobData.profilePath));

    // Now create alice2
    std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE2";
    details[ConfProperties::ALIAS] = "ALICE2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PATH] = aliceArchive;
    bobData.profilePath = {};
    alice2Data.profilePath = {};
    alice2Id = Manager::instance().addAccount(details);

    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] {
        return alice2Data.deviceAnnounced && !bobData.profilePath.empty() && !alice2Data.profilePath.empty();
    }));
}

void
SyncHistoryTest::testLastInteractionAfterClone()
{
    connectSignals();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    auto aliceMsgSize = aliceData.messages.size();
    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));

    // Start conversation
    libjami::sendMessage(bobId, aliceData.conversationId, std::string("Message 1"), "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&] { return aliceMsgSize + 2 == aliceData.messages.size(); }));
    libjami::sendMessage(bobId, aliceData.conversationId, std::string("Message 2"), "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&] { return aliceMsgSize + 3 == aliceData.messages.size(); }));
    libjami::sendMessage(bobId, aliceData.conversationId, std::string("Message 3"), "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&] { return aliceMsgSize + 4 == aliceData.messages.size(); }));

    auto msgId = aliceData.messages.rbegin()->id;
    libjami::setMessageDisplayed(aliceId, "swarm:" + aliceData.conversationId, msgId, 3);
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&] { return aliceData.sent; }));

    // Now create alice2
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
    aliceAccount->exportArchive(aliceArchive);
    std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE2";
    details[ConfProperties::ALIAS] = "ALICE2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PATH] = aliceArchive;
    alice2Id = Manager::instance().addAccount(details);

    // Check if conversation is ready
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() { return !alice2Data.conversationId.empty(); }));
    // Check that last displayed is synched
    auto membersInfos = libjami::getConversationMembers(alice2Id, alice2Data.conversationId);
    CPPUNIT_ASSERT(std::find_if(membersInfos.begin(),
                                membersInfos.end(),
                                [&](auto infos) { return infos["uri"] == aliceUri && infos["lastDisplayed"] == msgId; })
                   != membersInfos.end());
}

void
SyncHistoryTest::testLastInteractionAfterSomeMessages()
{
    connectSignals();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    // Creates alice2
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
    aliceAccount->exportArchive(aliceArchive);
    std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE2";
    details[ConfProperties::ALIAS] = "ALICE2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PATH] = aliceArchive;
    alice2Id = Manager::instance().addAccount(details);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return alice2Data.deviceAnnounced; }));
    auto getMessageFromBody = [](const auto& data, const auto& body) -> std::string {
        auto it = std::find_if(data.messages.begin(), data.messages.end(), [&](auto& msg) {
            return msg.body.find(CommitKey::BODY) != msg.body.end() && msg.body.at(CommitKey::BODY) == body;
        });
        if (it != data.messages.end()) {
            return it->id;
        }
        return {};
    };
    auto getMessage = [](const auto& data, const auto& mid) -> bool {
        return std::find_if(data.messages.begin(), data.messages.end(), [&](auto& msg) { return msg.id == mid; })
               != data.messages.end();
    };

    aliceAccount->addContact(bobUri);
    aliceAccount->sendTrustRequest(bobUri, {});
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived && !alice2Data.conversationId.empty(); }));

    CPPUNIT_ASSERT(bobAccount->acceptTrustRequest(aliceUri));
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&]() { return aliceData.members[bobUri] == 1 && alice2Data.members[bobUri] == 1; }));

    // Start conversation
    bobData.messages.clear();
    libjami::sendMessage(bobId, aliceData.conversationId, std::string("Message 1"), "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !getMessageFromBody(bobData, "Message 1").empty(); }));
    auto msgId = getMessageFromBody(bobData, "Message 1");
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return getMessage(aliceData, msgId) && getMessage(alice2Data, msgId); }));

    bobData.messages.clear();
    libjami::sendMessage(bobId, aliceData.conversationId, std::string("Message 2"), "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !getMessageFromBody(bobData, "Message 2").empty(); }));
    msgId = getMessageFromBody(bobData, "Message 2");
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&] { return getMessage(aliceData, msgId) && getMessage(alice2Data, msgId); }));

    bobData.messages.clear();
    libjami::sendMessage(bobId, aliceData.conversationId, std::string("Message 3"), "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !getMessageFromBody(bobData, "Message 3").empty(); }));
    msgId = getMessageFromBody(bobData, "Message 3");
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&] { return getMessage(aliceData, msgId) && getMessage(alice2Data, msgId); }));

    libjami::setMessageDisplayed(aliceId, "swarm:" + aliceData.conversationId, msgId, 3);
    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return aliceData.sent && alice2Data.sent; }));

    auto membersInfos = libjami::getConversationMembers(alice2Id, alice2Data.conversationId);
    CPPUNIT_ASSERT(std::find_if(membersInfos.begin(),
                                membersInfos.end(),
                                [&](auto infos) { return infos["uri"] == aliceUri && infos["lastDisplayed"] == msgId; })
                   != membersInfos.end());
}

} // namespace test
} // namespace jami

int
main(int argc, char** argv)
{
    CppUnit::TextUi::TestRunner runner;
    runner.addTest(CppUnit::TestFactoryRegistry::getRegistry(jami::test::SyncHistoryTest::name()).makeTest());
    return runner.run(argc > 1 ? argv[1] : "") ? 0 : 1;
}
