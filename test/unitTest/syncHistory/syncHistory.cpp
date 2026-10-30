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
#include "jamidht/account_manager.h"
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

using namespace libjami::Account;
using namespace std::literals::chrono_literals;

namespace jami {
namespace test {

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
    std::map<std::string, int64_t> deviceSyncs;
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
    void testContactMetadataPreservesAddedDate();
    void testContactReadditionUpdatesAddedDate();
    void testContactLinkSyncBeforeRemoval();
    void testContactLinkSyncAfterRemoval();
    void checkContactLinkSync(bool removalFirst);
    void testRecordDeviceSync();
    void testAccountSyncTimestamp();
    void testDeviceSyncTimestamp();
    void testCreateConversationThenSync();
    void testCreateConversationWithOnlineDevice();
    void testCreateConversationWithMessagesThenAddDevice();
    void testCreateMultipleConversationThenAddDevice();
    void testReceivesInviteThenAddDevice();
    void testRemoveConversationOnAllDevices();
    void testSyncCreateAccountExportDeleteReimportOldBackup();
    void testSyncCreateAccountExportDeleteReimportWithConvId();
    void testSyncCreateAccountExportDeleteReimportWithConvReq();
    void testSyncOneToOne();
    void testConversationRequestRemoved();
    void testProfileReceivedMultiDevice();
    void testLastInteractionAfterClone();
    void testLastInteractionAfterSomeMessages();

    CPPUNIT_TEST_SUITE(SyncHistoryTest);
    CPPUNIT_TEST(testContactMetadataPreservesAddedDate);
    CPPUNIT_TEST(testContactReadditionUpdatesAddedDate);
    CPPUNIT_TEST(testContactLinkSyncBeforeRemoval);
    CPPUNIT_TEST(testContactLinkSyncAfterRemoval);
    CPPUNIT_TEST(testRecordDeviceSync);
    CPPUNIT_TEST(testAccountSyncTimestamp);
    CPPUNIT_TEST(testDeviceSyncTimestamp);
    CPPUNIT_TEST(testCreateConversationThenSync);
    CPPUNIT_TEST(testCreateConversationWithOnlineDevice);
    CPPUNIT_TEST(testCreateConversationWithMessagesThenAddDevice);
    CPPUNIT_TEST(testCreateMultipleConversationThenAddDevice);
    CPPUNIT_TEST(testReceivesInviteThenAddDevice);
    CPPUNIT_TEST(testRemoveConversationOnAllDevices);
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
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::KnownDevicesChanged>(
        [&](const std::string& accountId, const std::map<std::string, std::string>&) {
            if (accountId != aliceId && accountId != alice2Id)
                return;
            auto account = Manager::instance().getAccount<JamiAccount>(accountId);
            if (!account)
                return;
            const auto devices = account->getDeviceList();
            std::lock_guard lock(mtx);
            auto& data = accountId == aliceId ? aliceData : alice2Data;
            for (const auto& device : devices) {
                auto timestamp = device.find("lastSync");
                if (timestamp != device.end() && std::stoll(timestamp->second) > 0)
                    data.deviceSyncs[device.at("id")] = std::stoll(timestamp->second);
            }
            cv.notify_one();
        }));
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
SyncHistoryTest::testContactMetadataPreservesAddedDate()
{
    auto alice = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto info = alice->accountManager()->getInfo();
    auto path = alice->getPath() / "contact-dates-test";
    std::filesystem::create_directories(path);
    ContactList::OnChangeCallback callbacks;
    callbacks.contactAdded = [](const std::string&, bool) {
    };
    callbacks.contactRemoved = [](const std::string&, bool) {
    };
    ContactList contacts(aliceId, info->identity.second->issuer, path, callbacks);
    const auto peer = dht::InfoHash::getRandom();
    Contact original;
    original.added = nowMs() - 24h;
    original.conversationId = "original";
    contacts.setContacts({{peer, original}});

    CPPUNIT_ASSERT(contacts.addContact(peer, true, "original"));
    CPPUNIT_ASSERT(contacts.getContactInfo(peer)->added == original.added);
    CPPUNIT_ASSERT(contacts.addContact(peer, true, "replacement"));
    CPPUNIT_ASSERT(contacts.getContactInfo(peer)->added == original.added);
    CPPUNIT_ASSERT(contacts.updateConversation(peer, "existing"));
    CPPUNIT_ASSERT(contacts.getContactInfo(peer)->added == original.added);
    CPPUNIT_ASSERT(contacts.removeContactConversation(peer));
    CPPUNIT_ASSERT(contacts.getContactInfo(peer)->added == original.added);

    ContactList reloaded(aliceId, info->identity.second->issuer, path, callbacks);
    reloaded.load();
    CPPUNIT_ASSERT(reloaded.getContactInfo(peer)->added == original.added);
    CPPUNIT_ASSERT(reloaded.getContactInfo(peer)->confirmed);
    CPPUNIT_ASSERT(reloaded.getContactInfo(peer)->conversationId.empty());
}

void
SyncHistoryTest::testContactReadditionUpdatesAddedDate()
{
    auto alice = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto info = alice->accountManager()->getInfo();
    auto path = alice->getPath() / "contact-readdition-test";
    std::filesystem::create_directories(path);
    ContactList::OnChangeCallback callbacks;
    callbacks.contactAdded = [](const std::string&, bool) {
    };
    callbacks.contactRemoved = [](const std::string&, bool) {
    };
    ContactList contacts(aliceId, info->identity.second->issuer, path, callbacks);
    const auto peer = dht::InfoHash::getRandom();
    Contact original;
    original.added = nowMs() - 24h;
    original.confirmed = true;
    original.conversationId = "original";
    contacts.setContacts({{peer, original}});

    CPPUNIT_ASSERT(contacts.removeContact(peer, false));
    const auto removed = *contacts.getContactInfo(peer);
    CPPUNIT_ASSERT(removed.added == original.added);
    CPPUNIT_ASSERT(!removed.isActive());
    CPPUNIT_ASSERT(contacts.addContact(peer, false, "replacement"));
    const auto readded = *contacts.getContactInfo(peer);
    CPPUNIT_ASSERT(readded.added > original.added);
    CPPUNIT_ASSERT(readded.removed == TimePoint {});
    CPPUNIT_ASSERT(readded.isActive());

    auto offline = original;
    offline.update(readded);
    CPPUNIT_ASSERT(offline.added == readded.added);
    CPPUNIT_ASSERT_EQUAL(readded.conversationId, offline.conversationId);
    offline.update(original);
    CPPUNIT_ASSERT(offline.added == readded.added);
    CPPUNIT_ASSERT_EQUAL(readded.conversationId, offline.conversationId);
}

void
SyncHistoryTest::testContactLinkSyncBeforeRemoval()
{
    checkContactLinkSync(false);
}

void
SyncHistoryTest::testContactLinkSyncAfterRemoval()
{
    checkContactLinkSync(true);
}

void
SyncHistoryTest::checkContactLinkSync(bool removalFirst)
{
    auto alice = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bob = Manager::instance().getAccount<JamiAccount>(bobId);
    const auto peer = dht::InfoHash(bob->getUsername());
    auto* module = alice->convModule();
    auto startUniqueConversation = [&] {
        // Initial commit IDs use second-resolution timestamps.
        const auto second = toSecondsSinceEpoch(nowMs());
        const auto deadline = std::chrono::steady_clock::now() + 2s;
        while (toSecondsSinceEpoch(nowMs()) == second && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(10ms);
        CPPUNIT_ASSERT(toSecondsSinceEpoch(nowMs()) != second);
        const auto id = module->startConversation(ConversationMode::ONE_TO_ONE, peer);
        CPPUNIT_ASSERT(!id.empty());
        return id;
    };
    const auto oldId = startUniqueConversation();
    const auto oldInfo = ConversationModule::convInfos(aliceId).at(oldId);
    Contact original;
    original.added = nowMs() - 24h;
    original.confirmed = true;
    original.conversationId = oldId;
    alice->accountManager()->getInfo()->contacts->setContacts({{peer, original}});

    SyncMsg removal;
    removal.c.emplace(oldId, oldInfo);
    removal.c.at(oldId).removed = nowMs();
    if (removalFirst)
        module->onSyncData(removal, alice->getUsername(), {});

    const auto newId = startUniqueConversation();
    SyncMsg replacement;
    replacement.c.emplace(newId, ConversationModule::convInfos(aliceId).at(newId));
    replacement.c.at(newId).created = std::max(replacement.c.at(newId).created, oldInfo.created + 1ms);
    module->onSyncData(replacement, alice->getUsername(), {});
    if (!removalFirst) {
        CPPUNIT_ASSERT_EQUAL(oldId, alice->getContactInfo(peer.toString())->conversationId);
        module->onSyncData(removal, alice->getUsername(), {});
    }

    auto contact = alice->getContactInfo(peer.toString());
    CPPUNIT_ASSERT(contact->added == original.added);
    CPPUNIT_ASSERT_EQUAL(newId, contact->conversationId);
    CPPUNIT_ASSERT(contact->isActive());
    SyncMsg stale;
    stale.c.emplace(oldId, oldInfo);
    module->onSyncData(stale, alice->getUsername(), {});
    CPPUNIT_ASSERT_EQUAL(newId, alice->getContactInfo(peer.toString())->conversationId);

    const auto concurrentId = startUniqueConversation();
    SyncMsg concurrent;
    concurrent.c.emplace(concurrentId, ConversationModule::convInfos(aliceId).at(concurrentId));
    module->onSyncData(concurrent, alice->getUsername(), {});
    CPPUNIT_ASSERT_EQUAL(newId, alice->getContactInfo(peer.toString())->conversationId);
    module->onSyncData(replacement, alice->getUsername(), {});
    CPPUNIT_ASSERT_EQUAL(newId, alice->getContactInfo(peer.toString())->conversationId);

    auto legacy = *contact;
    legacy.added = contact->added + 1ms;
    auto received = *contact;
    received.update(legacy);
    CPPUNIT_ASSERT(received.added == legacy.added);
    alice->removeContact(peer.toString(), false);
    module->onSyncData(replacement, alice->getUsername(), {});
    CPPUNIT_ASSERT(!alice->getContactInfo(peer.toString())->isActive());
}

void
SyncHistoryTest::testRecordDeviceSync()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto info = aliceAccount->accountManager()->getInfo();
    auto path = aliceAccount->getPath() / "device-sync-test";
    std::filesystem::create_directories(path);
    const auto device = info->identity.second->getLongId();
    const auto receivedAt = ContactList::time_point(std::chrono::milliseconds(1700000001123));
    const auto unknownDevice = dht::PkId::getRandom();
    size_t notifications = 0;
    std::unique_ptr<ContactList> contacts;
    ContactList::OnChangeCallback callbacks;
    callbacks.devicesChanged = [&](const std::map<dht::PkId, KnownDevice>& devices) {
        ++notifications;
        CPPUNIT_ASSERT_EQUAL(devices.size(), contacts->getKnownDevices().size());
    };
    contacts = std::make_unique<ContactList>(aliceId, info->identity.second->issuer, path, callbacks);
    CPPUNIT_ASSERT(contacts->foundAccountDevice(info->identity.second, "source"));
    notifications = 0;

    CPPUNIT_ASSERT(!contacts->recordDeviceSync(unknownDevice, receivedAt));
    CPPUNIT_ASSERT_EQUAL(size_t(0), notifications);
    CPPUNIT_ASSERT_EQUAL(size_t(1), contacts->getKnownDevices().size());

    CPPUNIT_ASSERT(contacts->recordDeviceSync(device, receivedAt));
    CPPUNIT_ASSERT_EQUAL(size_t(1), notifications);
    CPPUNIT_ASSERT(contacts->getKnownDevices().at(device).last_sync == receivedAt);
    CPPUNIT_ASSERT(!contacts->recordDeviceSync(device, receivedAt));
    CPPUNIT_ASSERT(!contacts->recordDeviceSync(device, receivedAt - 1ms));
    CPPUNIT_ASSERT_EQUAL(size_t(1), notifications);

    const auto nextReceipt = receivedAt + 1ms;
    CPPUNIT_ASSERT(contacts->recordDeviceSync(device, nextReceipt));
    CPPUNIT_ASSERT_EQUAL(size_t(2), notifications);
    CPPUNIT_ASSERT(contacts->getKnownDevices().at(device).last_sync == nextReceipt);

    ContactList reloaded(aliceId, info->identity.second->issuer, path, callbacks);
    reloaded.load();
    CPPUNIT_ASSERT(reloaded.getKnownDevices().at(device).last_sync == nextReceipt);
}

void
SyncHistoryTest::testAccountSyncTimestamp()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto manager = aliceAccount->accountManager();
    const auto device = manager->getInfo()->devicePk->getLongId();
    const auto receivedAt = AccountManager::clock::now();
    DeviceSync sync;
    sync.date = 1;
    manager->onSyncData(std::move(sync), device);
    const auto timestamp = manager->getKnownDevices().at(device).last_sync;
    CPPUNIT_ASSERT(timestamp >= receivedAt);

    manager->onSyncData(DeviceSync {}, device);
    CPPUNIT_ASSERT(manager->getKnownDevices().at(device).last_sync == timestamp);
}

void
SyncHistoryTest::testDeviceSyncTimestamp()
{
    connectSignals();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    const auto aliceDevice = aliceAccount->getVolatileAccountDetails().at(ConfProperties::DEVICE_ID);
    const auto started
        = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto aliceArchive = std::filesystem::current_path().string() + "/alice.gz";
    CPPUNIT_ASSERT(aliceAccount->exportArchive(aliceArchive));
    auto details = libjami::getAccountTemplate("RING");
    details[ConfProperties::DISPLAYNAME] = "ALICE2";
    details[ConfProperties::ALIAS] = "ALICE2";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PATH] = aliceArchive;
    alice2Id = Manager::instance().addAccount(details);

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return alice2Data.deviceSyncs.count(aliceDevice) > 0; }));
    const auto timestamp = alice2Data.deviceSyncs.at(aliceDevice);
    CPPUNIT_ASSERT(timestamp >= started);

    auto alice2Account = Manager::instance().getAccount<JamiAccount>(alice2Id);
    auto file = fileutils::loadFile("knownDevices", alice2Account->getPath());
    auto decoded = msgpack::unpack(reinterpret_cast<const char*>(file.data()), file.size());
    auto devices = decoded.get().as<std::map<dht::PkId, KnownDeviceData>>();
    const auto savedTimestamp = devices.at(dht::PkId(aliceDevice)).lastSyncMs;
    CPPUNIT_ASSERT(savedTimestamp >= timestamp * 1000);
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
    add_confirmed_contact(aliceId, bobId);
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
    add_confirmed_contact(bobId, aliceId);
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
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return alice2Data.conversationId == convId; }));

    auto bobMsgSize = bobData.messages.size();
    libjami::sendMessage(alice2Id, convId, std::string("hi"), "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobMsgSize + 1 == bobData.messages.size(); }));
}

void
SyncHistoryTest::testSyncCreateAccountExportDeleteReimportWithConvId()
{
    add_confirmed_contact(bobId, aliceId);
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
    add_confirmed_contact(aliceId, bobId);
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

    const auto bobUri = bobAccount->getUsername();
    aliceAccount->addContact(bobUri);
    const auto original = *aliceAccount->getContactInfo(bobUri);
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
    auto alice2 = Manager::instance().getAccount<JamiAccount>(alice2Id);
    CPPUNIT_ASSERT(alice2->getContactInfo(bobUri)->added == original.added);
    alice2Data.conversationId.clear();
    CPPUNIT_ASSERT(libjami::removeConversation(aliceId, original.conversationId));
    const auto replacement = *aliceAccount->getContactInfo(bobUri);
    CPPUNIT_ASSERT(replacement.conversationId != original.conversationId);
    CPPUNIT_ASSERT(replacement.added == original.added);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&] { return alice2Data.conversationId == replacement.conversationId; }));
    const auto synced = *alice2->getContactInfo(bobUri);
    CPPUNIT_ASSERT_EQUAL(replacement.conversationId, synced.conversationId);
    CPPUNIT_ASSERT(synced.added == original.added);
    CPPUNIT_ASSERT(ContactList::contactsFromPath(alice2->getPath()).at(dht::InfoHash(bobUri)).added == original.added);
}

void
SyncHistoryTest::testConversationRequestRemoved()
{
    add_confirmed_contact(aliceId, bobId);
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

CORE_TEST_RUNNER(jami::test::SyncHistoryTest::name())
