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
    void testFileTransfer();
    void testMultipleFileTransfer();
    void testConversationFileTransfer();
    void testFileTransferInConversation();
    void testReaskForTransfer();
    void testBadSha3sumOut();
    void testBadSha3sumIn();
    void testAskToMultipleParticipants();

    CPPUNIT_TEST_SUITE(FileTransferTest);
    CPPUNIT_TEST(testFileTransfer);
    CPPUNIT_TEST(testMultipleFileTransfer);
    CPPUNIT_TEST(testConversationFileTransfer);
    CPPUNIT_TEST(testFileTransferInConversation);
    CPPUNIT_TEST(testReaskForTransfer);
    CPPUNIT_TEST(testBadSha3sumOut);
    CPPUNIT_TEST(testBadSha3sumIn);
    CPPUNIT_TEST(testAskToMultipleParticipants);
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
FileTransferTest::testFileTransfer()
{
    std::this_thread::sleep_for(std::chrono::seconds(5));
    // TODO remove. This sleeps is because it take some time for the DHT to be connected
    // and account announced
    JAMI_INFO("Waiting....");
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::condition_variable cv2;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool transferWaiting = false, transferFinished = false;
    DRing::DataTransferId finalId;
    // Watch signals
    confHandlers.insert(DRing::exportable_callback<DRing::DataTransferSignal::DataTransferEvent>(
        [&](const std::string& accountId, const std::string&, const long unsigned int& id, int code) {
            if (accountId == bobId
                && code == static_cast<int>(DRing::DataTransferEventCode::wait_host_acceptance)) {
                transferWaiting = true;
                finalId = id;
                cv.notify_one();
            } else if (accountId == aliceId
                       && code == static_cast<int>(DRing::DataTransferEventCode::finished)) {
                transferFinished = true;
                finalId = id;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);

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
    info.peer = bobUri;
    info.path = "SEND";
    info.displayName = "SEND";
    info.bytesProgress = 0;
    CPPUNIT_ASSERT(DRing::sendFile(info, id) == DRing::DataTransferError::success);

    cv.wait_for(lk, std::chrono::seconds(30));
    CPPUNIT_ASSERT(transferWaiting);

    auto rcv_path = "RECV";
    CPPUNIT_ASSERT(DRing::acceptFileTransfer(bobId, aliceUri, finalId, rcv_path, 0)
                   == DRing::DataTransferError::success);

    // Wait 2 times, both sides will got a finished status
    cv.wait_for(lk, std::chrono::seconds(30));
    cv.wait_for(lk, std::chrono::seconds(30));
    CPPUNIT_ASSERT(transferFinished);

    CPPUNIT_ASSERT(compare(info.path, rcv_path));

    // TODO FIX ME. The ICE take some time to stop and it doesn't seems to like
    // when stopping the daemon and removing the accounts to soon.
    std::remove("SEND");
    std::remove("RECV");
    JAMI_INFO("Waiting....");
    std::this_thread::sleep_for(std::chrono::seconds(3));
}

void
FileTransferTest::testMultipleFileTransfer()
{
    std::this_thread::sleep_for(std::chrono::seconds(5));
    // TODO remove. This sleeps is because it take some time for the DHT to be connected
    // and account announced
    JAMI_INFO("Waiting....");
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::condition_variable cv2;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool transferWaiting = false, transferFinished = false;
    DRing::DataTransferId finalId;
    // Watch signals
    confHandlers.insert(DRing::exportable_callback<DRing::DataTransferSignal::DataTransferEvent>(
        [&](const std::string& accountId, const std::string&, const long unsigned int& id, int code) {
            if (accountId == bobId
                && code == static_cast<int>(DRing::DataTransferEventCode::wait_host_acceptance)) {
                transferWaiting = true;
                finalId = id;
                cv.notify_one();
            } else if (accountId == aliceId
                       && code == static_cast<int>(DRing::DataTransferEventCode::finished)) {
                transferFinished = true;
                finalId = id;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);

    // Create file to send
    std::ofstream sendFile("SEND");
    CPPUNIT_ASSERT(sendFile.is_open());
    // Avoid ASAN error on big alloc   sendFile << std::string("A", 64000);
    for (int i = 0; i < 64000; ++i)
        sendFile << "A";
    sendFile.close();
    std::ofstream sendFile2("SEND2");
    CPPUNIT_ASSERT(sendFile2.is_open());
    for (int i = 0; i < 64000; ++i)
        sendFile2 << "B";
    sendFile2.close();

    // Send first File
    DRing::DataTransferInfo info;
    uint64_t id;
    info.accountId = aliceAccount->getAccountID();
    info.peer = bobUri;
    info.path = "SEND";
    info.displayName = "SEND";
    info.bytesProgress = 0;
    CPPUNIT_ASSERT(DRing::sendFile(info, id) == DRing::DataTransferError::success);

    cv.wait_for(lk, std::chrono::seconds(30));
    CPPUNIT_ASSERT(transferWaiting);
    transferWaiting = false;

    auto rcv_path = "RECV";
    CPPUNIT_ASSERT(DRing::acceptFileTransfer(bobId, aliceUri, finalId, rcv_path, 0)
                   == DRing::DataTransferError::success);

    // Wait 2 times, both sides will got a finished status
    cv.wait_for(lk, std::chrono::seconds(30));
    cv.wait_for(lk, std::chrono::seconds(30));
    CPPUNIT_ASSERT(transferFinished);

    CPPUNIT_ASSERT(compare(info.path, rcv_path));

    // Send File
    DRing::DataTransferInfo info2;
    info2.accountId = aliceAccount->getAccountID();
    info2.peer = bobUri;
    info2.path = "SEND2";
    info2.displayName = "SEND2";
    info2.bytesProgress = 0;
    CPPUNIT_ASSERT(DRing::sendFile(info2, id) == DRing::DataTransferError::success);

    cv.wait_for(lk, std::chrono::seconds(30));
    CPPUNIT_ASSERT(transferWaiting);

    rcv_path = "RECV2";
    CPPUNIT_ASSERT(DRing::acceptFileTransfer(bobId, aliceUri, finalId, rcv_path, 0)
                   == DRing::DataTransferError::success);

    // Wait 2 times, both sides will got a finished status
    cv.wait_for(lk, std::chrono::seconds(30));
    cv.wait_for(lk, std::chrono::seconds(30));
    CPPUNIT_ASSERT(transferFinished);

    CPPUNIT_ASSERT(compare(info2.path, rcv_path));

    // TODO FIX ME. The ICE take some time to stop and it doesn't seems to like
    // when stopping the daemon and removing the accounts to soon.
    std::remove("SEND");
    std::remove("SEND2");
    std::remove("RECV");
    std::remove("RECV2");
    JAMI_INFO("Waiting....");
    std::this_thread::sleep_for(std::chrono::seconds(3));
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

    auto messageReceivedAlice = 0;
    auto messageReceivedBob = 0;
    auto messageReceivedCarla = 0;
    auto requestReceived = 0;
    auto conversationReady = 0;
    long unsigned int hostAcceptanceBob = {0}, hostAcceptanceCarla = {0};
    std::vector<long unsigned int> peerAcceptance = {}, finished = {};
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> /*message*/) {
            if (accountId == aliceId)
                messageReceivedAlice += 1;
            if (accountId == bobId)
                messageReceivedBob += 1;
            if (accountId == carlaId)
                messageReceivedCarla += 1;
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

    messageReceivedAlice = 0;
    bobAccount->acceptConversationRequest(convId);
    carlaAccount->acceptConversationRequest(convId);
    cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return conversationReady == 3 && messageReceivedAlice >= 2;
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

    cv.wait_for(lk, std::chrono::seconds(45), [&]() {
        return peerAcceptance.size() == 1 && hostAcceptanceBob != 0 && hostAcceptanceCarla != 0;
    });

    CPPUNIT_ASSERT(DRing::acceptFileTransfer(bobId, convId, hostAcceptanceBob, "RCV", 0)
                   == DRing::DataTransferError::success);
    CPPUNIT_ASSERT(DRing::acceptFileTransfer(carlaId, convId, hostAcceptanceCarla, "RCV2", 0)
                   == DRing::DataTransferError::success);

    cv.wait_for(lk, std::chrono::seconds(45), [&]() { return finished.size() == 3; });

    std::remove("SEND");
    std::remove("RCV");
    std::remove("RCV2");

    DRing::unregisterSignalHandlers();
    std::this_thread::sleep_for(std::chrono::seconds(10));
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
    auto messageBobReceived = 0, messageAliceReceived = 0;
    bool requestReceived = false;
    bool conversationReady = false;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> /*message*/) {
            if (accountId == bobId) {
                messageBobReceived += 1;
            } else {
                messageAliceReceived += 1;
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
    bool transferWaiting = false, transferFinished = false;
    DRing::DataTransferId finalId;
    // Watch signals
    confHandlers.insert(DRing::exportable_callback<DRing::DataTransferSignal::DataTransferEvent>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            const long unsigned int& id,
            int code) {
            if (accountId == bobId && conversationId == convId
                && code == static_cast<int>(DRing::DataTransferEventCode::wait_host_acceptance)) {
                transferWaiting = true;
                finalId = id;
                cv.notify_one();
            } else if (accountId == aliceId && conversationId == convId
                       && code == static_cast<int>(DRing::DataTransferEventCode::finished)) {
                transferFinished = true;
                finalId = id;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);

    aliceAccount->addConversationMember(convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));

    bobAccount->acceptConversationRequest(convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return conversationReady && messageAliceReceived == 1;
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
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return transferWaiting; }));
    std::this_thread::sleep_for(std::chrono::seconds(30));

    CPPUNIT_ASSERT(DRing::acceptFileTransfer(bobId, convId, id, "RECV", 0)
                   == DRing::DataTransferError::success);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return transferFinished; }));

    std::remove("SEND");
    std::remove("RECV");
    DRing::unregisterSignalHandlers();
    std::this_thread::sleep_for(std::chrono::seconds(5));
}

void
FileTransferTest::testReaskForTransfer()
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
    bool transferWaiting = false, transferFinished = false;
    DRing::DataTransferId finalId;
    // Watch signals
    confHandlers.insert(DRing::exportable_callback<DRing::DataTransferSignal::DataTransferEvent>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            const long unsigned int& id,
            int code) {
            if (accountId == bobId && conversationId == convId
                && code == static_cast<int>(DRing::DataTransferEventCode::wait_host_acceptance)) {
                transferWaiting = true;
                finalId = id;
                cv.notify_one();
            } else if (accountId == aliceId && conversationId == convId
                       && code == static_cast<int>(DRing::DataTransferEventCode::finished)) {
                transferFinished = true;
                finalId = id;
                cv.notify_one();
            }
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
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return transferWaiting; }));
    std::this_thread::sleep_for(std::chrono::seconds(5));

    CPPUNIT_ASSERT(DRing::cancelDataTransfer(bobId, convId, id)
                   == DRing::DataTransferError::success);

    std::this_thread::sleep_for(std::chrono::seconds(10));
    CPPUNIT_ASSERT(mid != "");
    transferWaiting = false;
    transferFinished = false;
    DRing::askForTransfer(bobId, "swarm:" + convId, mid, "RECV");

    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return transferFinished; }));
    CPPUNIT_ASSERT(fileutils::isFile("RECV"));

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
    bool transferWaiting = false, transferFinished = false;
    DRing::DataTransferId finalId;
    // Watch signals
    confHandlers.insert(DRing::exportable_callback<DRing::DataTransferSignal::DataTransferEvent>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            const long unsigned int& id,
            int code) {
            if (accountId == bobId && conversationId == convId
                && code == static_cast<int>(DRing::DataTransferEventCode::wait_host_acceptance)) {
                transferWaiting = true;
                finalId = id;
                cv.notify_one();
            } else if (accountId == aliceId && conversationId == convId
                       && code == static_cast<int>(DRing::DataTransferEventCode::finished)) {
                transferFinished = true;
                finalId = id;
                cv.notify_one();
            }
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
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return transferWaiting; }));
    std::this_thread::sleep_for(std::chrono::seconds(5));

    CPPUNIT_ASSERT(DRing::cancelDataTransfer(bobId, convId, id)
                   == DRing::DataTransferError::success);

    std::this_thread::sleep_for(std::chrono::seconds(10));
    // modifiy file
    sendFile = std::ofstream("SEND");
    CPPUNIT_ASSERT(sendFile.is_open());
    // Avoid ASAN error on big alloc   sendFile << std::string("B", 64000);
    for (int i = 0; i < 64000; ++i)
        sendFile << "B";
    sendFile.close();

    CPPUNIT_ASSERT(mid != "");
    transferWaiting = false;
    transferFinished = false;
    DRing::askForTransfer(bobId, "swarm:" + convId, mid, "RECV");

    // The file transfer will not be sent as modified
    CPPUNIT_ASSERT(!cv.wait_for(lk, std::chrono::seconds(30), [&]() { return transferFinished; }));

    std::remove("SEND");
    DRing::unregisterSignalHandlers();
    std::this_thread::sleep_for(std::chrono::seconds(5));
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
    bool transferWaiting = false, transferInFinished = false, transferOutFinished = false;
    DRing::DataTransferId finalId;
    // Watch signals
    confHandlers.insert(DRing::exportable_callback<DRing::DataTransferSignal::DataTransferEvent>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            const long unsigned int& id,
            int code) {
            finalId = id;
            if (accountId == bobId && conversationId == convId
                && code == static_cast<int>(DRing::DataTransferEventCode::wait_host_acceptance)) {
                transferWaiting = true;
            } else if (accountId == bobId && conversationId == convId
                       && code == static_cast<int>(DRing::DataTransferEventCode::finished)) {
                transferInFinished = true;
            } else if (accountId == aliceId && conversationId == convId
                       && code == static_cast<int>(DRing::DataTransferEventCode::finished)) {
                transferOutFinished = true;
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
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return transferWaiting; }));
    std::this_thread::sleep_for(std::chrono::seconds(5));

    CPPUNIT_ASSERT(DRing::cancelDataTransfer(bobId, convId, id)
                   == DRing::DataTransferError::success);

    std::this_thread::sleep_for(std::chrono::seconds(10));
    // modifiy file
    sendFile = std::ofstream("SEND");
    CPPUNIT_ASSERT(sendFile.is_open());
    // Avoid ASAN error on big alloc   sendFile << std::string("B", 64000);
    for (int i = 0; i < 64000; ++i)
        sendFile << "B";
    sendFile.close();

    CPPUNIT_ASSERT(mid != "");
    transferWaiting = false;
    transferInFinished = false;
    transferOutFinished = false;
    DRing::askForTransfer(bobId, "swarm:" + convId, mid, "RECV");
    aliceAccount->noSha3sumVerification(true);

    CPPUNIT_ASSERT(!cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return transferOutFinished && transferInFinished;
    }));
    CPPUNIT_ASSERT(!fileutils::isFile("RECV"));

    std::remove("SEND");
    DRing::unregisterSignalHandlers();
    std::this_thread::sleep_for(std::chrono::seconds(5));
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
            if (accountId == bobId || accountId == carlaId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    bool transferWaiting = false, transferFinished = false, transferCarlaFinished = false;
    long unsigned int hostAcceptanceBob = {0}, hostAcceptanceCarla = {0};
    DRing::DataTransferId finalId;
    // Watch signals
    confHandlers.insert(DRing::exportable_callback<DRing::DataTransferSignal::DataTransferEvent>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            const long unsigned int& id,
            int code) {
            if (code == static_cast<int>(DRing::DataTransferEventCode::wait_host_acceptance)) {
                if (accountId == bobId)
                    hostAcceptanceBob = id;
                else if (accountId == carlaId)
                    hostAcceptanceCarla = id;
            } else if (accountId == bobId && conversationId == convId
                       && code
                              == static_cast<int>(
                                  DRing::DataTransferEventCode::wait_host_acceptance)) {
                transferWaiting = true;
                hostAcceptanceBob = id;
            } else if (accountId == bobId && conversationId == convId
                       && code == static_cast<int>(DRing::DataTransferEventCode::finished)) {
                transferFinished = true;
                finalId = id;
            } else if (accountId == carlaId && conversationId == convId
                       && code == static_cast<int>(DRing::DataTransferEventCode::finished)) {
                transferCarlaFinished = true;
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

    JAMI_ERR("@@@");

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
        return hostAcceptanceBob != 0 && hostAcceptanceCarla != 0;
    }));

    CPPUNIT_ASSERT(DRing::cancelDataTransfer(bobId, convId, hostAcceptanceBob)
                   == DRing::DataTransferError::success);
    CPPUNIT_ASSERT(DRing::acceptFileTransfer(carlaId, convId, hostAcceptanceCarla, "RECV2", 0)
                   == DRing::DataTransferError::success);
    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(30), [&]() { return transferCarlaFinished; }));
    CPPUNIT_ASSERT(fileutils::isFile("RECV2"));

    std::this_thread::sleep_for(std::chrono::seconds(10));
    CPPUNIT_ASSERT(mid != "");
    transferFinished = false;
    DRing::askForTransfer(bobId, "swarm:" + convId, mid, "RECV");

    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return transferFinished; }));
    CPPUNIT_ASSERT(fileutils::isFile("RECV"));

    std::remove("SEND");
    std::remove("RECV");
    std::remove("RECV2");
    DRing::unregisterSignalHandlers();
    std::this_thread::sleep_for(std::chrono::seconds(5));
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::FileTransferTest::name())