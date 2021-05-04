/*
 *  Copyright (C)2020-2021 Savoir-faire Linux Inc.
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <condition_variable>
#include <string>

#include "fileutils.h"
#include "manager.h"
#include "jamidht/connectionmanager.h"
#include "jamidht/jamiaccount.h"
#include "../../test_runner.h"
#include "dring.h"
#include "data_transfer.h"
#include "dring/datatransfer_interface.h"
#include "account_const.h"

using namespace DRing::Account;

namespace jami {
namespace test {

class FileTransferTest : public CppUnit::TestFixture
{
public:
    FileTransferTest()
    {
        // Init daemon
        DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(DRing::start("dring-sample.yml"));
    }
    ~FileTransferTest() { DRing::fini(); }
    static std::string name() { return "Call"; }
    bool compare(const std::string& fileA, const std::string& fileB) const;
    void setUp();
    void tearDown();

    std::string aliceId;
    std::string bobId;
    std::string carlaId;

private:
    void testConversationFileTransfer();
    void testFileTransferInConversation();
    void testBadSha3sumOut();
    void testBadSha3sumIn();
    void testAskToMultipleParticipants();
    void testCancelTransfer();

    CPPUNIT_TEST_SUITE(FileTransferTest);
    CPPUNIT_TEST(testConversationFileTransfer);
    CPPUNIT_TEST(testFileTransferInConversation);
    CPPUNIT_TEST(testBadSha3sumOut);
    CPPUNIT_TEST(testBadSha3sumIn);
    CPPUNIT_TEST(testAskToMultipleParticipants);
    CPPUNIT_TEST(testCancelTransfer);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(FileTransferTest, FileTransferTest::name());

bool
FileTransferTest::compare(const std::string& fileA, const std::string& fileB) const
{
    std::ifstream f1(fileA, std::ifstream::binary | std::ifstream::ate);
    std::ifstream f2(fileB, std::ifstream::binary | std::ifstream::ate);

    if (f1.fail() || f2.fail() || f1.tellg() != f2.tellg()) {
        return false;
    }

    f1.seekg(0, std::ifstream::beg);
    f2.seekg(0, std::ifstream::beg);
    return std::equal(std::istreambuf_iterator<char>(f1.rdbuf()),
                      std::istreambuf_iterator<char>(),
                      std::istreambuf_iterator<char>(f2.rdbuf()));
}

void
FileTransferTest::setUp()
{
    std::map<std::string, std::string> details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "ALICE";
    details[ConfProperties::ALIAS] = "ALICE";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = "";
    aliceId = Manager::instance().addAccount(details);

    details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "BOB";
    details[ConfProperties::ALIAS] = "BOB";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = "";
    bobId = Manager::instance().addAccount(details);

    details = DRing::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "CARLA";
    details[ConfProperties::ALIAS] = "CARLA";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PIN] = "";
    details[ConfProperties::ARCHIVE_PATH] = "";
    carlaId = Manager::instance().addAccount(details);

    JAMI_INFO("Initialize account...");
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string&, const std::map<std::string, std::string>&) {
                bool ready = false;
                auto details = aliceAccount->getVolatileAccountDetails();
                auto daemonStatus = details[DRing::Account::ConfProperties::Registration::STATUS];
                ready = (daemonStatus == "REGISTERED");
                details = bobAccount->getVolatileAccountDetails();
                daemonStatus = details[DRing::Account::ConfProperties::Registration::STATUS];
                ready &= (daemonStatus == "REGISTERED");
                details = carlaAccount->getVolatileAccountDetails();
                daemonStatus = details[DRing::Account::ConfProperties::Registration::STATUS];
                ready &= (daemonStatus == "REGISTERED");
            }));
    DRing::registerSignalHandlers(confHandlers);
    cv.wait_for(lk, std::chrono::seconds(30));
    DRing::unregisterSignalHandlers();
}

void
FileTransferTest::tearDown()
{
    JAMI_INFO("Remove created accounts...");

    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    auto currentAccSize = Manager::instance().getAccountList().size();
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::AccountsChanged>([&]() {
            if (Manager::instance().getAccountList().size() <= currentAccSize - 3) {
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);

    Manager::instance().removeAccount(aliceId, true);
    Manager::instance().removeAccount(bobId, true);
    Manager::instance().removeAccount(carlaId, true);
    // Because cppunit is not linked with dbus, just poll if removed
    cv.wait_for(lk, std::chrono::seconds(30));

    DRing::unregisterSignalHandlers();
}

void
FileTransferTest::testConversationFileTransfer()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    aliceAccount->trackBuddyPresence(carlaUri, true);

    // Enable carla
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(
            [&](const std::string&, const std::map<std::string, std::string>&) {
                auto details = carlaAccount->getVolatileAccountDetails();
                auto daemonStatus = details[DRing::Account::ConfProperties::Registration::STATUS];
                if (daemonStatus == "REGISTERED") {
                    cv.notify_one();
                }
            }));
    DRing::registerSignalHandlers(confHandlers);

    Manager::instance().sendRegister(carlaId, true);
    cv.wait_for(lk, std::chrono::seconds(30));
    confHandlers.clear();
    DRing::unregisterSignalHandlers();

    auto requestReceived = 0;
    auto conversationReady = 0;
    auto memberJoined = 0;
    std::string tidBob, tidCarla;
    long unsigned int hostAcceptanceBob = {0}, hostAcceptanceCarla = {0};
    std::vector<long unsigned int> peerAcceptance = {}, finished = {};
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> message) {
            if (message["type"] == "application/data-transfer+json") {
                if (accountId == bobId) {
                    tidBob = message["id"];
                } else if (accountId == carlaId) {
                    tidCarla = message["id"];
                }
            } else if (accountId == aliceId && message["type"] == "member"
                       && message["action"] == "join") {
                memberJoined += 1;
            }
            cv.notify_one();
        }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived += 1;
                if (requestReceived >= 2)
                    cv.notify_one();
            }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& /*accountId*/, const std::string& /* conversationId */) {
            conversationReady += 1;
            if (conversationReady >= 3)
                cv.notify_one();
        }));
    confHandlers.insert(DRing::exportable_callback<DRing::DataTransferSignal::DataTransferEvent>(
        [&](const std::string& accountId, const std::string&, const long unsigned int& id, int code) {
            if (code == static_cast<int>(DRing::DataTransferEventCode::wait_host_acceptance)) {
                if (accountId == bobId)
                    hostAcceptanceBob = id;
                else if (accountId == carlaId)
                    hostAcceptanceCarla = id;
                cv.notify_one();
            } else if (code
                       == static_cast<int>(DRing::DataTransferEventCode::wait_peer_acceptance)) {
                peerAcceptance.emplace_back(id);
                cv.notify_one();
            } else if (code == static_cast<int>(DRing::DataTransferEventCode::finished)) {
                finished.emplace_back(id);
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);

    auto convId = aliceAccount->startConversation();

    aliceAccount->addConversationMember(convId, bobUri);
    aliceAccount->addConversationMember(convId, carlaUri);
    cv.wait_for(lk, std::chrono::seconds(60), [&]() { return requestReceived == 2; });

    bobAccount->acceptConversationRequest(convId);
    carlaAccount->acceptConversationRequest(convId);
    cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return conversationReady == 3 && memberJoined == 2;
    });

    // Send file
    std::ofstream sendFile("SEND");
    CPPUNIT_ASSERT(sendFile.is_open());
    // Avoid ASAN error on big alloc   sendFile << std::string("A", 64000);
    for (int i = 0; i < 64000; ++i)
        sendFile << "A";
    sendFile.close();

    // Send File
    DRing::DataTransferInfo info;
    uint64_t id;
    info.accountId = aliceAccount->getAccountID();
    info.conversationId = convId;
    info.path = "SEND";
    info.displayName = "SEND";
    info.bytesProgress = 0;

    CPPUNIT_ASSERT(DRing::sendFile(info, id) == DRing::DataTransferError::success);

    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(45), [&]() {
        return !tidBob.empty() && !tidCarla.empty();
    }));

    DRing::downloadFile(bobId, "swarm:" + convId, tidBob, "RCV");
    DRing::downloadFile(carlaId, "swarm:" + convId, tidCarla, "RCV2");

    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(45), [&]() { return finished.size() == 3; }));

    std::remove("SEND");
    std::remove("RCV");
    std::remove("RCV2");

    DRing::unregisterSignalHandlers();
}

void
FileTransferTest::testFileTransferInConversation()
{
    std::this_thread::sleep_for(std::chrono::seconds(5));
    // TODO remove. This sleeps is because it take some time for the DHT to be connected
    // and account announced
    JAMI_INFO("Waiting....");
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    auto convId = aliceAccount->startConversation();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool requestReceived = false;
    bool conversationReady = false;
    bool bobJoined = false;
    std::string tidBob;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> message) {
            if (message["type"] == "application/data-transfer+json") {
                if (accountId == bobId) {
                    tidBob = message["id"];
                }
            }
            if (accountId == aliceId && message["type"] == "member" && message["action"] == "join") {
                bobJoined = true;
            }
            cv.notify_one();
        }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    bool transferAFinished = false, transferBFinished = false;
    // Watch signals
    confHandlers.insert(DRing::exportable_callback<DRing::DataTransferSignal::DataTransferEvent>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            const long unsigned int& id,
            int code) {
            if (code == static_cast<int>(DRing::DataTransferEventCode::finished)
                && conversationId == convId) {
                if (accountId == aliceId)
                    transferAFinished = true;
                else if (accountId == bobId)
                    transferBFinished = true;
            }
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);

    aliceAccount->addConversationMember(convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));

    bobAccount->acceptConversationRequest(convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return conversationReady && bobJoined;
    }));

    // Create file to send
    std::ofstream sendFile("SEND");
    CPPUNIT_ASSERT(sendFile.is_open());
    // Avoid ASAN error on big alloc   sendFile << std::string("A", 64000);
    for (int i = 0; i < 64000; ++i)
        sendFile << "A";
    sendFile.close();

    // Send File
    DRing::DataTransferInfo info;
    uint64_t id;
    info.accountId = aliceAccount->getAccountID();
    info.conversationId = convId;
    info.path = "SEND";
    info.displayName = "SEND";
    info.bytesProgress = 0;
    CPPUNIT_ASSERT(DRing::sendFile(info, id) == DRing::DataTransferError::success);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return !tidBob.empty(); }));

    DRing::downloadFile(bobId, "swarm:" + convId, tidBob, "RECV");
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return transferAFinished && transferBFinished;
    }));

    std::remove("SEND");
    std::remove("RECV");
    DRing::unregisterSignalHandlers();
    std::this_thread::sleep_for(std::chrono::seconds(5));
}

void
FileTransferTest::testBadSha3sumOut()
{
    std::this_thread::sleep_for(std::chrono::seconds(5));
    // TODO remove. This sleeps is because it take some time for the DHT to be connected
    // and account announced
    JAMI_INFO("Waiting....");
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    auto convId = aliceAccount->startConversation();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool requestReceived = false;
    bool conversationReady = false;
    bool memberJoin = false;
    std::string mid = {};
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> message) {
            if (accountId == bobId) {
                if (message["type"] == "application/data-transfer+json") {
                    mid = message["id"];
                }
            }
            cv.notify_one();
        }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    bool transferAFinished = false, transferBFinished = false;
    // Watch signals
    confHandlers.insert(DRing::exportable_callback<DRing::DataTransferSignal::DataTransferEvent>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            const long unsigned int& id,
            int code) {
            if (conversationId == convId
                && code == static_cast<int>(DRing::DataTransferEventCode::finished)) {
                if (accountId == aliceId)
                    transferAFinished = true;
                if (accountId == bobId)
                    transferBFinished = true;
            }
            cv.notify_one();
        }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationMemberEvent>(
            [&](const std::string& accountId,
                const std::string& conversationId,
                const std::string& uri,
                int event) {
                if (accountId == aliceId && conversationId == convId && uri == bobUri
                    && event == 1) {
                    memberJoin = true;
                }
                cv.notify_one();
            }));
    DRing::registerSignalHandlers(confHandlers);

    aliceAccount->addConversationMember(convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));

    bobAccount->acceptConversationRequest(convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return conversationReady && memberJoin;
    }));

    // Create file to send
    std::ofstream sendFile("SEND");
    CPPUNIT_ASSERT(sendFile.is_open());
    // Avoid ASAN error on big alloc   sendFile << std::string("A", 64000);
    for (int i = 0; i < 64000; ++i)
        sendFile << "A";
    sendFile.close();

    // Send File
    DRing::DataTransferInfo info;
    uint64_t id;
    info.accountId = aliceAccount->getAccountID();
    info.conversationId = convId;
    info.path = "SEND";
    info.displayName = "SEND";
    info.bytesProgress = 0;
    CPPUNIT_ASSERT(DRing::sendFile(info, id) == DRing::DataTransferError::success);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return !mid.empty(); }));

    // modifiy file
    sendFile = std::ofstream("SEND");
    CPPUNIT_ASSERT(sendFile.is_open());
    // Avoid ASAN error on big alloc   sendFile << std::string("B", 64000);
    for (int i = 0; i < 64000; ++i)
        sendFile << "B";
    sendFile.close();

    DRing::downloadFile(bobId, "swarm:" + convId, mid, "RECV");

    // The file transfer will not be sent as modified
    CPPUNIT_ASSERT(!cv.wait_for(lk, std::chrono::seconds(30), [&]() { return transferAFinished; }));
    CPPUNIT_ASSERT(!cv.wait_for(lk, std::chrono::seconds(30), [&]() { return transferBFinished; }));

    std::remove("SEND");
    DRing::unregisterSignalHandlers();
}

void
FileTransferTest::testBadSha3sumIn()
{
    std::this_thread::sleep_for(std::chrono::seconds(5));
    // TODO remove. This sleeps is because it take some time for the DHT to be connected
    // and account announced
    JAMI_INFO("Waiting....");
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    auto convId = aliceAccount->startConversation();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool requestReceived = false;
    bool conversationReady = false;
    bool memberJoin = false;
    std::string mid = {};
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> message) {
            if (accountId == bobId) {
                if (message["type"] == "application/data-transfer+json") {
                    mid = message["id"];
                }
            }
            cv.notify_one();
        }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    bool transferAFinished = false, transferBFinished = false;
    // Watch signals
    confHandlers.insert(DRing::exportable_callback<DRing::DataTransferSignal::DataTransferEvent>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            const long unsigned int& id,
            int code) {
            if (conversationId == convId
                && code == static_cast<int>(DRing::DataTransferEventCode::finished)) {
                if (accountId == aliceId)
                    transferAFinished = true;
                if (accountId == bobId)
                    transferBFinished = true;
            }
            cv.notify_one();
        }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationMemberEvent>(
            [&](const std::string& accountId,
                const std::string& conversationId,
                const std::string& uri,
                int event) {
                if (accountId == aliceId && conversationId == convId && uri == bobUri
                    && event == 1) {
                    memberJoin = true;
                }
                cv.notify_one();
            }));
    DRing::registerSignalHandlers(confHandlers);

    aliceAccount->addConversationMember(convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));

    bobAccount->acceptConversationRequest(convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return conversationReady && memberJoin;
    }));

    // Create file to send
    std::ofstream sendFile("SEND");
    CPPUNIT_ASSERT(sendFile.is_open());
    // Avoid ASAN error on big alloc   sendFile << std::string("A", 64000);
    for (int i = 0; i < 64000; ++i)
        sendFile << "A";
    sendFile.close();

    // Send File
    DRing::DataTransferInfo info;
    uint64_t id;
    info.accountId = aliceAccount->getAccountID();
    info.conversationId = convId;
    info.path = "SEND";
    info.displayName = "SEND";
    info.bytesProgress = 0;
    aliceAccount->noSha3sumVerification(true);
    CPPUNIT_ASSERT(DRing::sendFile(info, id) == DRing::DataTransferError::success);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return !mid.empty(); }));

    // modifiy file
    sendFile = std::ofstream("SEND");
    CPPUNIT_ASSERT(sendFile.is_open());
    // Avoid ASAN error on big alloc   sendFile << std::string("B", 64000);
    for (int i = 0; i < 64000; ++i)
        sendFile << "B";
    sendFile.close();

    DRing::downloadFile(bobId, "swarm:" + convId, mid, "RECV");

    // The file transfer will be sent but refused by bob
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return transferAFinished; }));
    CPPUNIT_ASSERT(!cv.wait_for(lk, std::chrono::seconds(30), [&]() { return transferBFinished; }));

    std::remove("SEND");
    DRing::unregisterSignalHandlers();
}

void
FileTransferTest::testAskToMultipleParticipants()
{
    std::this_thread::sleep_for(std::chrono::seconds(5));
    // TODO remove. This sleeps is because it take some time for the DHT to be connected
    // and account announced
    JAMI_INFO("Waiting....");
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto carlaUri = carlaAccount->getUsername();
    auto convId = aliceAccount->startConversation();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool requestReceived = false;
    bool conversationReady = false;
    bool memberJoin = false;
    std::string bobTid, carlaTid;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> message) {
            if (message["type"] == "application/data-transfer+json") {
                if (accountId == bobId) {
                    bobTid = message["id"];
                } else if (accountId == carlaId) {
                    carlaTid = message["id"];
                }
            }
            cv.notify_one();
        }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId || accountId == carlaId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    bool transferBFinished = false, transferCFinished = false;
    // Watch signals
    confHandlers.insert(DRing::exportable_callback<DRing::DataTransferSignal::DataTransferEvent>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            const long unsigned int& id,
            int code) {
            if (conversationId == convId
                && code == static_cast<int>(DRing::DataTransferEventCode::finished)) {
                if (accountId == carlaId)
                    transferCFinished = true;
                if (accountId == bobId)
                    transferBFinished = true;
            }
            cv.notify_one();
        }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationMemberEvent>(
            [&](const std::string& accountId,
                const std::string& conversationId,
                const std::string& uri,
                int event) {
                if (accountId == aliceId && conversationId == convId
                    && (uri == bobUri || uri == carlaUri) && event == 1) {
                    memberJoin = true;
                }
                cv.notify_one();
            }));
    DRing::registerSignalHandlers(confHandlers);

    aliceAccount->addConversationMember(convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));

    bobAccount->acceptConversationRequest(convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return conversationReady && memberJoin;
    }));

    requestReceived = false;
    conversationReady = false;
    memberJoin = false;

    aliceAccount->addConversationMember(convId, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));

    carlaAccount->acceptConversationRequest(convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return conversationReady && memberJoin;
    }));

    // Create file to send
    std::ofstream sendFile("SEND");
    CPPUNIT_ASSERT(sendFile.is_open());
    // Avoid ASAN error on big alloc   sendFile << std::string("A", 64000);
    for (int i = 0; i < 64000; ++i)
        sendFile << "A";
    sendFile.close();

    // Send File
    DRing::DataTransferInfo info;
    uint64_t id;
    info.accountId = aliceAccount->getAccountID();
    info.conversationId = convId;
    info.path = "SEND";
    info.displayName = "SEND";
    info.bytesProgress = 0;
    CPPUNIT_ASSERT(DRing::sendFile(info, id) == DRing::DataTransferError::success);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return !bobTid.empty() && !carlaTid.empty();
    }));

    DRing::downloadFile(carlaId, "swarm:" + convId, carlaTid, "RECV2");
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return transferCFinished; }));
    CPPUNIT_ASSERT(fileutils::isFile("RECV2"));

    DRing::downloadFile(bobId, "swarm:" + convId, bobTid, "RECV");
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return transferBFinished; }));
    CPPUNIT_ASSERT(fileutils::isFile("RECV"));

    std::remove("SEND");
    std::remove("RECV");
    std::remove("RECV2");
    DRing::unregisterSignalHandlers();
}

void
FileTransferTest::testCancelTransfer()
{
    std::this_thread::sleep_for(std::chrono::seconds(5));
    // TODO remove. This sleeps is because it take some time for the DHT to be connected
    // and account announced
    JAMI_INFO("Waiting....");
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    auto convId = aliceAccount->startConversation();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool requestReceived = false;
    bool conversationReady = false;
    bool bobJoined = false;
    std::string tidBob;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> message) {
            if (message["type"] == "application/data-transfer+json") {
                if (accountId == bobId) {
                    tidBob = message["id"];
                }
            }
            if (accountId == aliceId && message["type"] == "member" && message["action"] == "join") {
                bobJoined = true;
            }
            cv.notify_one();
        }));
    confHandlers.insert(
        DRing::exportable_callback<DRing::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& /*accountId*/,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                requestReceived = true;
                cv.notify_one();
            }));
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    bool transferAFinished = false, transferBOngoing = false, transferBFinished = false;
    // Watch signals
    confHandlers.insert(DRing::exportable_callback<DRing::DataTransferSignal::DataTransferEvent>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            const long unsigned int& id,
            int code) {
            if (code == static_cast<int>(DRing::DataTransferEventCode::ongoing)
                && conversationId == convId) {
                if (accountId == bobId)
                    transferBOngoing = true;
            } else if (code > static_cast<int>(DRing::DataTransferEventCode::finished)
                       && conversationId == convId) {
                if (accountId == aliceId)
                    transferAFinished = true;
                else if (accountId == bobId)
                    transferBFinished = true;
            }
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);

    aliceAccount->addConversationMember(convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));

    bobAccount->acceptConversationRequest(convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return conversationReady && bobJoined;
    }));

    // Create file to send
    std::ofstream sendFile("SEND");
    CPPUNIT_ASSERT(sendFile.is_open());
    // Avoid ASAN error on big alloc   sendFile << std::string("A", 64000);
    for (int i = 0; i < 1000000; ++i)
        sendFile << "A";
    sendFile.close();

    // Send File
    DRing::DataTransferInfo info;
    uint64_t id;
    info.accountId = aliceAccount->getAccountID();
    info.conversationId = convId;
    info.path = "SEND";
    info.displayName = "SEND";
    info.bytesProgress = 0;
    CPPUNIT_ASSERT(DRing::sendFile(info, id) == DRing::DataTransferError::success);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return !tidBob.empty(); }));

    auto fileId = DRing::downloadFile(bobId, "swarm:" + convId, tidBob, "RECV");
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return transferBOngoing; }));
    DRing::cancelDataTransfer(bobId, convId, fileId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return transferAFinished && transferBFinished;
    }));
    CPPUNIT_ASSERT(!fileutils::isFile("RECV"));

    std::remove("SEND");
    DRing::unregisterSignalHandlers();
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::FileTransferTest::name())