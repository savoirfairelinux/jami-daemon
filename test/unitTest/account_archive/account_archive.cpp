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

#include "manager.h"
#include "jamidht/conversationrepository.h"
#include "jamidht/gitserver.h"
#include "jamidht/jamiaccount.h"
#include "jamidht/accountarchive.h"
#include "jamidht/archive_account_manager.h"
#include "jamidht/sync_module.h"
#include "json_utils.h"
#include "../../test_runner.h"
#include "archiver.h"
#include "base64.h"
#include "jami.h"
#include "fileutils.h"
#include "account_const.h"
#include "account_schema.h"
#include "common.h"

#include <dhtnet/connectionmanager.h>
#include <dhtnet/channel_socket.h>

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <git2.h>
#include <condition_variable>
#include <functional>
#include <optional>
#include <string>
#include <fstream>
#include <streambuf>
#include <filesystem>
#include <thread>

using namespace std::string_literals;
using namespace std::literals::chrono_literals;
using namespace libjami::Account;

namespace jami {
namespace test {

class AccountArchiveTest : public CppUnit::TestFixture
{
public:
    AccountArchiveTest()
    {
        // Init daemon
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("dring-sample.yml"));
    }
    ~AccountArchiveTest() { libjami::fini(); }
    static std::string name() { return "AccountArchive"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    std::string bobId;
    std::string bob2Id;

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    bool aliceReady = false;

private:
    class SyncSocket : public dhtnet::ChannelSocket
    {
    public:
        SyncSocket()
            : ChannelSocket({}, "sync", 1)
        {}
        void setOnRecv(RecvCb&& cb) override { receive = std::move(cb); }
        std::size_t write(const ValueType* data, std::size_t length, std::error_code& ec) override
        {
            if (onWrite)
                onWrite(data, length);
            ec.clear();
            return length;
        }
        std::function<void(const ValueType*, std::size_t)> onWrite;
        RecvCb receive;
    };

    std::map<std::string, std::string> readMetadata(const std::string& accountId)
    {
        std::map<std::string, std::string> result;
        CPPUNIT_ASSERT(libjami::getAccountMetadata(accountId, result));
        return result;
    }

    void testExportImportNoPassword();
    void testExportImportNoPasswordDoubleGunzip();
    void testExportImportPassword();
    void testExportImportPasswordDoubleGunzip();
    // void testExportDht();
    // void testExportDhtWrongPassword();
    void testChangePassword();
    void testChangeDhtPort();
    void testAccountMetadataArchive();
    void testAccountMetadataArchiveReadFailure();
    void testAccountMetadataSync();
    void testAccountMetadataOnlySync();
    void testAccountMetadataFailureDoesNotBlockSync();
    void testAccountMetadataUpdatesStayConnectedOnly();

    CPPUNIT_TEST_SUITE(AccountArchiveTest);
    CPPUNIT_TEST(testExportImportNoPassword);
    CPPUNIT_TEST(testExportImportNoPasswordDoubleGunzip);
    CPPUNIT_TEST(testExportImportPassword);
    CPPUNIT_TEST(testExportImportPasswordDoubleGunzip);
    // CPPUNIT_TEST(testExportDht);
    // CPPUNIT_TEST(testExportDhtWrongPassword);
    CPPUNIT_TEST(testChangePassword);
    CPPUNIT_TEST(testChangeDhtPort);
    CPPUNIT_TEST(testAccountMetadataArchive);
    CPPUNIT_TEST(testAccountMetadataArchiveReadFailure);
    CPPUNIT_TEST(testAccountMetadataSync);
    CPPUNIT_TEST(testAccountMetadataOnlySync);
    CPPUNIT_TEST(testAccountMetadataFailureDoesNotBlockSync);
    CPPUNIT_TEST(testAccountMetadataUpdatesStayConnectedOnly);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(AccountArchiveTest, AccountArchiveTest::name());

void
AccountArchiveTest::testAccountMetadataOnlySync()
{
    auto alice = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto manager = alice->accountManager();
    const auto device = manager->getInfo()->devicePk->getLongId();
    Metadata metadata;
    metadata.entries["setting"] = {toMillisecondsSinceEpoch(nowMs()), std::string(64, 'a'), "received"};
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> packer(buffer);
    packer.pack_map(2);
    packer.pack(std::string("date"));
    packer.pack(uint64_t(1));
    packer.pack(std::string("metadata"));
    packer.pack(metadata);
    auto sync = msgpack::unpack(buffer.data(), buffer.size()).get().as<DeviceSync>();
    const auto receivedAt = AccountManager::clock::now();
    manager->onSyncData(std::move(sync), device);
    const auto receipt = manager->getKnownDevices().at(device).last_sync;
    CPPUNIT_ASSERT(receipt >= receivedAt);
    CPPUNIT_ASSERT(manager->accountMetadata().state() == metadata);
    const auto snapshot = manager->getSyncData();
    CPPUNIT_ASSERT(snapshot.metadata == metadata);

    DeviceSync undated;
    undated.metadata = metadata;
    ++undated.metadata.entries.at("setting").timestampMs;
    undated.metadata.entries.at("setting").value = "ignored";
    manager->onSyncData(std::move(undated), device);
    CPPUNIT_ASSERT(manager->accountMetadata().state() == metadata);
    CPPUNIT_ASSERT(manager->getKnownDevices().at(device).last_sync == receipt);
    manager->onSyncData(DeviceSync {}, device);
    CPPUNIT_ASSERT(manager->getKnownDevices().at(device).last_sync == receipt);

    DeviceSync invalid;
    invalid.date = 1;
    invalid.metadata.entries["setting"] = {0, std::string(64, 'a'), "invalid"};
    const auto failedReceivedAt = AccountManager::clock::now();
    manager->onSyncData(std::move(invalid), device);
    CPPUNIT_ASSERT(manager->getKnownDevices().at(device).last_sync >= failedReceivedAt);
    CPPUNIT_ASSERT(manager->accountMetadata().state() == metadata);
    DeviceSync mixed;
    mixed.date = 1;
    mixed.metadata.entries["setting"] = {0, std::string(64, 'a'), "invalid"};
    const dht::InfoHash peer(std::string(40, 'b'));
    Contact contact;
    contact.removed = nowMs();
    mixed.peers.emplace(peer, contact);
    manager->onSyncData(std::move(mixed), DeviceId(std::string(64, 'a')));
    const auto contacts = manager->getInfo()->contacts->getContacts();
    CPPUNIT_ASSERT(contacts.count(peer));
    CPPUNIT_ASSERT(contacts.at(peer).removed == contact.removed);
    CPPUNIT_ASSERT(manager->accountMetadata().state() == metadata);
}

void
AccountArchiveTest::testAccountMetadataFailureDoesNotBlockSync()
{
    auto alice = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bob = Manager::instance().getAccount<JamiAccount>(bobId);
    auto socket = std::make_shared<SyncSocket>();
    alice->syncModule()->cacheSyncConnection(std::shared_ptr<dhtnet::ChannelSocket>(socket),
                                             alice->getUsername(),
                                             DeviceId(std::string(64, 'a')));

    SyncMsg mixed;
    mixed.ds.date = 1;
    mixed.ds.metadata.entries["setting"] = {0, std::string(64, 'a'), "invalid"};
    ConversationRequest request;
    request.conversationId = std::string(40, 'a');
    request.from = bob->getUsername();
    request.received = nowMs();
    mixed.cr.emplace(request.conversationId, request);
    SyncMsg following;
    request.conversationId = std::string(40, 'b');
    following.cr.emplace(request.conversationId, request);
    msgpack::sbuffer buffer;
    msgpack::pack(buffer, mixed);
    msgpack::pack(buffer, following);
    const auto consumed = socket->receive(reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size());
    const auto requests = ConversationModule::convRequests(aliceId);
    CPPUNIT_ASSERT(requests.count(std::string(40, 'a')));
    CPPUNIT_ASSERT(requests.count(std::string(40, 'b')));
    CPPUNIT_ASSERT_EQUAL(static_cast<ssize_t>(buffer.size()), consumed);
    CPPUNIT_ASSERT(readMetadata(aliceId).empty());
}

void
AccountArchiveTest::testAccountMetadataUpdatesStayConnectedOnly()
{
    auto alice = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto* sync = alice->syncModule();
    cv.wait_for(lk, 2s, [] { return false; });
    const auto version = sync->currentVersion();
    const DeviceId disconnected(std::string(64, 'a'));
    sync->markSynced(disconnected, version);
    CPPUNIT_ASSERT(!sync->isConnected(disconnected));
    CPPUNIT_ASSERT(!sync->needsSync(disconnected));
    struct SentMetadata
    {
        std::mutex mutex;
        std::condition_variable cv;
        std::optional<uint64_t> date;
    };
    auto sent = std::make_shared<SentMetadata>();
    auto socket = std::make_shared<SyncSocket>();
    socket->onWrite = [sent](const auto* data, std::size_t length) {
        auto msg = msgpack::unpack(reinterpret_cast<const char*>(data), length).get().as<SyncMsg>();
        if (!msg.affectsList() && msg.ds.metadata.entries.count("setting")
            && msg.ds.metadata.entries.at("setting").value == "changed") {
            std::lock_guard lock(sent->mutex);
            sent->date = msg.ds.date;
            sent->cv.notify_one();
        }
    };
    const DeviceId connected(std::string(64, 'b'));
    sync->cacheSyncConnection(std::shared_ptr<dhtnet::ChannelSocket>(socket), alice->getUsername(), connected);
    lk.unlock();
    CPPUNIT_ASSERT(libjami::setAccountMetadata(aliceId, {{"setting", "changed"}}));
    {
        std::unique_lock lock(sent->mutex);
        CPPUNIT_ASSERT(sent->cv.wait_for(lock, 30s, [&] { return sent->date.has_value(); }));
        CPPUNIT_ASSERT(*sent->date != 0);
    }
    DeviceSync incoming;
    incoming.date = 1;
    incoming.metadata.entries["remote"] = {toMillisecondsSinceEpoch(nowMs()), std::string(64, 'a'), "received"};
    alice->accountManager()->onSyncData(std::move(incoming), disconnected);
    lk.lock();
    cv.wait_for(lk, 2s, [] { return false; });
    CPPUNIT_ASSERT_EQUAL(version, sync->currentVersion());
    CPPUNIT_ASSERT(!sync->needsSync(disconnected));
    CPPUNIT_ASSERT(!sync->isConnected(disconnected));
}

void
AccountArchiveTest::testAccountMetadataArchive()
{
    auto alice = Manager::instance().getAccount<JamiAccount>(aliceId);
    const std::map<std::string, std::string> metadata {{"jami.channels.v1/channel/name", "Équipe"},
                                                       {"jami.channels.v1/channel/members/uri", "0"},
                                                       {"jami.channels.v1/removed/deleted", "1"}};
    CPPUNIT_ASSERT(libjami::setAccountMetadata(aliceId, metadata));
    CPPUNIT_ASSERT(readMetadata(aliceId) == metadata);
    CPPUNIT_ASSERT(readMetadata(bobId).empty());
    CPPUNIT_ASSERT(!libjami::setAccountMetadata("missing-account", metadata));
    CPPUNIT_ASSERT(alice->exportArchive("metadata-export.gz"));
    AccountArchive exported {std::filesystem::path("metadata-export.gz")};
    CPPUNIT_ASSERT(alice->accountManager()->accountMetadata().state() == exported.metadata);
    const auto serialized = exported.serialize();
    AccountArchive roundtrip {std::string_view(serialized)};
    CPPUNIT_ASSERT(roundtrip.metadata == exported.metadata);
    CPPUNIT_ASSERT(!roundtrip.config.count("accountMetadata"));
    Json::Value json;
    CPPUNIT_ASSERT(json::parse(serialized, json));
    json.removeMember("accountMetadata");
    AccountArchive oldArchive {std::string_view(json::toString(json))};
    CPPUNIT_ASSERT(oldArchive.metadata.entries.empty());
    for (const auto& invalid : {Json::Value("not valid packed metadata"),
                                Json::Value(Json::objectValue),
                                Json::Value(base64::encode(std::string_view("corrupt")))}) {
        json["accountMetadata"] = invalid;
        AccountArchive decoded {std::string_view(json::toString(json))};
        CPPUNIT_ASSERT(decoded.metadata.entries.empty());
        CPPUNIT_ASSERT(decoded.id.first && decoded.id.second);
        CPPUNIT_ASSERT(decoded.id.second->getId() == exported.id.second->getId());
    }

    auto details = libjami::getAccountTemplate("RING");
    details[ConfProperties::ARCHIVE_PATH] = "metadata-export.gz";
    auto importedId = Manager::instance().addAccount(details);
    wait_for_announcement_of(importedId);
    auto imported = Manager::instance().getAccount<JamiAccount>(importedId);
    CPPUNIT_ASSERT(imported->currentDeviceId() != alice->currentDeviceId());
    CPPUNIT_ASSERT(readMetadata(importedId) == metadata);
    CPPUNIT_ASSERT(imported->accountManager()->accountMetadata().state() == exported.metadata);
    std::remove("metadata-export.gz");
    wait_for_removal_of(importedId);
}

void
AccountArchiveTest::testAccountMetadataArchiveReadFailure()
{
    const std::filesystem::path root = "metadata-archive-read-failure";
    CPPUNIT_ASSERT(std::filesystem::create_directory(root));
    auto cleanup = std::shared_ptr<void>(nullptr, [root](void*) { std::filesystem::remove_all(root); });
    auto alice = Manager::instance().getAccount<JamiAccount>(aliceId);
    CPPUNIT_ASSERT(libjami::setAccountMetadata(aliceId, {{"setting", "value"}}));
    CPPUNIT_ASSERT(alice->exportArchive((root / "archive.gz").string()));
    {
        std::ofstream file(root / "accountMetadata", std::ios::binary);
        file << "corrupt";
    }
    ArchiveAccountManager manager(
        "metadata-archive-read-failure",
        root,
        [] { return std::map<std::string, std::string> {}; },
        [](DeviceSync&&) {},
        "archive.gz",
        "");
    CPPUNIT_ASSERT(manager.exportArchive((root / "export.gz").string(), "", ""));
    AccountArchive exported {root / "export.gz"};
    CPPUNIT_ASSERT(exported.metadata.entries.empty());
    CPPUNIT_ASSERT_EQUAL(uintmax_t(7), std::filesystem::file_size(root / "accountMetadata"));
}

void
AccountArchiveTest::testAccountMetadataSync()
{
    auto alice = Manager::instance().getAccount<JamiAccount>(aliceId);
    CPPUNIT_ASSERT(alice->exportArchive("metadata-sync.gz"));
    auto details = libjami::getAccountTemplate("RING");
    details[ConfProperties::ARCHIVE_PATH] = "metadata-sync.gz";
    auto importedId = Manager::instance().addAccount(details);
    wait_for_announcement_of(importedId);
    std::remove("metadata-sync.gz");
    auto imported = Manager::instance().getAccount<JamiAccount>(importedId);
    const DeviceId aliceDevice(alice->currentDeviceId());
    const DeviceId importedDevice(imported->currentDeviceId());
    auto connected = [&] {
        return alice->syncModule()->isConnected(importedDevice) && imported->syncModule()->isConnected(aliceDevice);
    };
    lk.unlock();
    const auto deadline = std::chrono::steady_clock::now() + 30s;
    while (!connected() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(50ms);
    lk.lock();
    CPPUNIT_ASSERT(connected());

    const std::string key = "jami.channels.v1/channel/name";
    auto received = std::make_shared<bool>(false);
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::AccountMetadataChanged>(
        [this, importedId, key, received](const std::string& id, const std::map<std::string, std::string>& values) {
            std::lock_guard guard(mtx);
            if (id == importedId && values.count(key) && values.at(key) == "Synchronized")
                *received = true;
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);
    // Signals are synchronous for native handlers; do not hold the fixture mutex while updating.
    lk.unlock();
    CPPUNIT_ASSERT(libjami::setAccountMetadata(aliceId, {{key, "Synchronized"}}));
    lk.lock();
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return *received; }));
    CPPUNIT_ASSERT_EQUAL(std::string("Synchronized"), readMetadata(importedId).at(key));
    CPPUNIT_ASSERT(readMetadata(bobId).empty());
    libjami::unregisterSignalHandlers();
    confHandlers.clear();
    wait_for_removal_of(importedId);
}

void
AccountArchiveTest::setUp()
{
    auto actors = load_actors_and_wait_for_announcement("actors/alice-bob-password.yml");
    aliceId = actors["alice"];
    bobId = actors["bob"];

    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
        [&](const std::string& accountId, const std::map<std::string, std::string>& details) {
            if (accountId != aliceId) {
                return;
            }
            try {
                aliceReady |= accountId == aliceId
                              && details.at(jami::Conf::CONFIG_ACCOUNT_REGISTRATION_STATUS) == "REGISTERED"
                              && details.at(libjami::Account::VolatileProperties::DEVICE_ANNOUNCED) == "true";
            } catch (const std::out_of_range&) {
            }
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);
}

void
AccountArchiveTest::tearDown()
{
    if (!bob2Id.empty())
        wait_for_removal_of({aliceId, bob2Id, bobId});
    else
        wait_for_removal_of({aliceId, bobId});
}

void
AccountArchiveTest::testExportImportNoPassword()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    const auto& deviceCertificate = aliceAccount->identity().second;
    const auto& accountCertificate = deviceCertificate->issuer;
    CPPUNIT_ASSERT(accountCertificate && accountCertificate->issuer);
    CPPUNIT_ASSERT_EQUAL(static_cast<int>(GNUTLS_PK_RSA),
                         gnutls_x509_crt_get_pk_algorithm(deviceCertificate->cert, nullptr));
    CPPUNIT_ASSERT_EQUAL(static_cast<int>(GNUTLS_PK_RSA),
                         gnutls_x509_crt_get_pk_algorithm(accountCertificate->cert, nullptr));
    CPPUNIT_ASSERT_EQUAL(static_cast<int>(GNUTLS_PK_EC),
                         gnutls_x509_crt_get_pk_algorithm(accountCertificate->issuer->cert, nullptr));

    CPPUNIT_ASSERT(aliceAccount->exportArchive("test.gz"));

    std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
    details[ConfProperties::ARCHIVE_PATH] = "test.gz";

    auto accountId = jami::Manager::instance().addAccount(details);
    wait_for_announcement_of(accountId);
    auto alice2Account = Manager::instance().getAccount<JamiAccount>(accountId);
    CPPUNIT_ASSERT(alice2Account->getUsername() == aliceAccount->getUsername());
    const auto& importedAccountCertificate = alice2Account->identity().second->issuer;
    CPPUNIT_ASSERT(importedAccountCertificate && importedAccountCertificate->issuer);
    CPPUNIT_ASSERT(importedAccountCertificate->issuer->getId() == accountCertificate->issuer->getId());
    std::remove("test.gz");
    wait_for_removal_of(accountId);
}

void
AccountArchiveTest::testExportImportNoPasswordDoubleGunzip()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);

    CPPUNIT_ASSERT(aliceAccount->exportArchive("test.gz"));
    auto dat = fileutils::loadFile("test.gz");
    archiver::compressGzip(dat, "test.gz");

    std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
    details[ConfProperties::ARCHIVE_PATH] = "test.gz";

    auto accountId = jami::Manager::instance().addAccount(details);
    wait_for_announcement_of(accountId);
    auto alice2Account = Manager::instance().getAccount<JamiAccount>(accountId);
    CPPUNIT_ASSERT(alice2Account->getUsername() == aliceAccount->getUsername());
    std::remove("test.gz");
    wait_for_removal_of(accountId);
}

void
AccountArchiveTest::testExportImportPassword()
{
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);

    CPPUNIT_ASSERT(bobAccount->exportArchive("test.gz", "password", "test"));

    std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
    details[ConfProperties::ARCHIVE_PATH] = "test.gz";
    details[ConfProperties::ARCHIVE_PASSWORD] = "test";

    auto accountId = jami::Manager::instance().addAccount(details);
    wait_for_announcement_of(accountId);
    auto bob2Account = Manager::instance().getAccount<JamiAccount>(accountId);
    CPPUNIT_ASSERT(bob2Account->getUsername() == bobAccount->getUsername());
    std::remove("test.gz");
    wait_for_removal_of(accountId);
}

void
AccountArchiveTest::testExportImportPasswordDoubleGunzip()
{
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);

    CPPUNIT_ASSERT(bobAccount->exportArchive("test.gz", "password", "test"));
    auto dat = fileutils::loadFile("test.gz");
    archiver::compressGzip(dat, "test.gz");

    std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
    details[ConfProperties::ARCHIVE_PATH] = "test.gz";
    details[ConfProperties::ARCHIVE_PASSWORD] = "test";

    auto accountId = jami::Manager::instance().addAccount(details);
    wait_for_announcement_of(accountId);
    auto bob2Account = Manager::instance().getAccount<JamiAccount>(accountId);
    CPPUNIT_ASSERT(bob2Account->getUsername() == bobAccount->getUsername());
    std::remove("test.gz");
    wait_for_removal_of(accountId);
}

void
AccountArchiveTest::testChangePassword()
{
    // Test incorrect password, should fail
    CPPUNIT_ASSERT(!libjami::changeAccountPassword(aliceId, "wrong", "new"));
    // Test correct password, should succeed
    CPPUNIT_ASSERT(libjami::changeAccountPassword(aliceId, "", "new"));
    // Now it should fail
    CPPUNIT_ASSERT(!libjami::changeAccountPassword(aliceId, "", "new"));
    // Remove password again (should succeed)
    CPPUNIT_ASSERT(libjami::changeAccountPassword(aliceId, "new", ""));
}

void
AccountArchiveTest::testChangeDhtPort()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);

    std::map<std::string, std::string> newDetails;
    newDetails[libjami::Account::ConfProperties::DHT_PORT] = "1412";
    aliceReady = false;
    libjami::setAccountDetails(aliceId, newDetails);
    cv.wait_for(lk, 20s, [&] { return aliceReady; });

    CPPUNIT_ASSERT(aliceAccount->dht()->getBoundPort() == 1412);

    newDetails[libjami::Account::ConfProperties::DHT_PORT] = "1413";
    aliceReady = false;
    libjami::setAccountDetails(aliceId, newDetails);
    cv.wait_for(lk, 20s, [&] { return aliceReady; });

    CPPUNIT_ASSERT(aliceAccount->dht()->getBoundPort() == 1413);
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::AccountArchiveTest::name())
