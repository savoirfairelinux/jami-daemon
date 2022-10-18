/*
 *  Copyright (C) 2020-2022 Savoir-faire Linux Inc.
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
#include <filesystem>

#include "fileutils.h"
#include "manager.h"
#include "dhtnet/connectionmanager.h"
#include "jamidht/jamiaccount.h"
#include "../../test_runner.h"
#include "jami.h"
#include "data_transfer.h"
#include "jami/datatransfer_interface.h"
#include "account_const.h"
#include "common.h"

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
            CPPUNIT_ASSERT(DRing::start("jami-sample.yml"));
    }
    ~FileTransferTest() { DRing::fini(); }
    static std::string name() { return "Call"; }
    bool compare(const std::string& fileA, const std::string& fileB) const;
    void setUp();
    void tearDown();

    std::string aliceId;
    std::string bobId;
    std::string carlaId;

    std::string sendPath {std::filesystem::current_path() / "SEND"};
    std::string recvPath {std::filesystem::current_path() / "RECV"};
    std::string recv2Path {std::filesystem::current_path() / "RECV2"};

private:
    void testFileTransfer();
    void testDataTransferInfo();
    void testMultipleFileTransfer();
    void testConversationFileTransfer();
    void testFileTransferInConversation();
    void testVcfFileTransferInConversation();
    void testBadSha3sumOut();
    void testBadSha3sumIn();
    void testAskToMultipleParticipants();
    void testCancelInTransfer();
    void testCancelOutTransfer();
    void testTransferInfo();

    CPPUNIT_TEST_SUITE(FileTransferTest);
    CPPUNIT_TEST(testFileTransfer);
    CPPUNIT_TEST(testDataTransferInfo);
    CPPUNIT_TEST(testMultipleFileTransfer);
    CPPUNIT_TEST(testConversationFileTransfer);
    CPPUNIT_TEST(testFileTransferInConversation);
    CPPUNIT_TEST(testVcfFileTransferInConversation);
    CPPUNIT_TEST(testBadSha3sumOut);
    CPPUNIT_TEST(testBadSha3sumIn);
    CPPUNIT_TEST(testAskToMultipleParticipants);
    CPPUNIT_TEST(testCancelInTransfer);
    CPPUNIT_TEST(testTransferInfo);
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
    auto actors = load_actors_and_wait_for_announcement("actors/alice-bob-carla.yml");
    aliceId = actors["alice"];
    bobId = actors["bob"];
    carlaId = actors["carla"];
}

void
FileTransferTest::tearDown()
{
    wait_for_removal_of({aliceId, bobId, carlaId});
}

void
FileTransferTest::testFileTransfer()
{
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
    std::string finalId;
    // Watch signals
    confHandlers.insert(DRing::exportable_callback<DRing::DataTransferSignal::DataTransferEvent>(
        [&](const std::string& accountId,
            const std::string&,
            const std::string&,
            const std::string& fileId,
            int code) {
            if (accountId == bobId
                && code == static_cast<int>(DRing::DataTransferEventCode::wait_host_acceptance)) {
                transferWaiting = true;
                finalId = fileId;
                cv.notify_one();
            } else if (accountId == aliceId
                       && code == static_cast<int>(DRing::DataTransferEventCode::finished)) {
                transferFinished = true;
                finalId = fileId;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);

    // Create file to send
    auto sendVcfPath = sendPath + ".vcf";
    auto recvVcfPath = recvPath + ".vcf";
    std::ofstream sendFile(sendVcfPath);
    CPPUNIT_ASSERT(sendFile.is_open());
    sendFile << std::string(64000, 'A');
    sendFile.close();

    // Send File
    DRing::DataTransferInfo info;
    uint64_t id;
    info.accountId = aliceAccount->getAccountID();
    info.peer = bobUri;
    info.path = sendVcfPath;
    info.displayName = "SEND.vcf";
    info.bytesProgress = 0;
    CPPUNIT_ASSERT(DRing::sendFileLegacy(info, id) == DRing::DataTransferError::success);

    cv.wait_for(lk, std::chrono::seconds(30));
    CPPUNIT_ASSERT(transferWaiting);

    CPPUNIT_ASSERT(DRing::acceptFileTransfer(bobId, finalId, recvVcfPath)
                   == DRing::DataTransferError::success);

    // Wait 2 times, both sides will got a finished status
    cv.wait_for(lk, std::chrono::seconds(30));
    cv.wait_for(lk, std::chrono::seconds(30));
    CPPUNIT_ASSERT(transferFinished);

    CPPUNIT_ASSERT(compare(info.path, recvVcfPath));

    // TODO FIX ME. The ICE take some time to stop and it doesn't seems to like
    // when stopping the daemon and removing the accounts to soon.
    std::remove(sendVcfPath.c_str());
    std::remove(recvVcfPath.c_str());
}

void
FileTransferTest::testDataTransferInfo()
{
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
    std::string finalId;
    // Watch signals
    confHandlers.insert(DRing::exportable_callback<DRing::DataTransferSignal::DataTransferEvent>(
        [&](const std::string& accountId,
            const std::string&,
            const std::string&,
            const std::string& fileId,
            int code) {
            if (accountId == bobId
                && code == static_cast<int>(DRing::DataTransferEventCode::wait_host_acceptance)) {
                transferWaiting = true;
                finalId = fileId;
                cv.notify_one();
            } else if (accountId == aliceId
                       && code == static_cast<int>(DRing::DataTransferEventCode::finished)) {
                transferFinished = true;
                finalId = fileId;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);

    // Create file to send
    std::ofstream sendFile(sendPath);
    CPPUNIT_ASSERT(sendFile.is_open());
    sendFile << std::string(64000, 'A');
    sendFile.close();

    // Send File
    DRing::DataTransferInfo info;
    uint64_t id;
    info.accountId = aliceAccount->getAccountID();
    info.peer = bobUri;
    info.path = sendPath;
    info.displayName = "SEND";
    info.bytesProgress = 0;
    CPPUNIT_ASSERT(DRing::sendFileLegacy(info, id) == DRing::DataTransferError::success);

    cv.wait_for(lk, std::chrono::seconds(30));
    CPPUNIT_ASSERT(transferWaiting);

    CPPUNIT_ASSERT(DRing::dataTransferInfo(bobId, std::to_string(id), info)
                   == DRing::DataTransferError::success);

    CPPUNIT_ASSERT(info.lastEvent == DRing::DataTransferEventCode::wait_host_acceptance);
    CPPUNIT_ASSERT(info.bytesProgress == 0);
    CPPUNIT_ASSERT(info.totalSize == 64000);

    CPPUNIT_ASSERT(DRing::dataTransferInfo(aliceId, std::to_string(id), info)
                   == DRing::DataTransferError::success);

    CPPUNIT_ASSERT(info.lastEvent == DRing::DataTransferEventCode::wait_peer_acceptance);
    CPPUNIT_ASSERT(info.bytesProgress == 0);
    CPPUNIT_ASSERT(info.totalSize == 64000);

    CPPUNIT_ASSERT(DRing::acceptFileTransfer(bobId, finalId, recvPath)
                   == DRing::DataTransferError::success);

    // Wait 2 times, both sides will got a finished status
    cv.wait_for(lk, std::chrono::seconds(30));
    cv.wait_for(lk, std::chrono::seconds(30));
    CPPUNIT_ASSERT(transferFinished);

    CPPUNIT_ASSERT(compare(info.path, recvPath));

    CPPUNIT_ASSERT(DRing::dataTransferInfo(bobId, std::to_string(id), info)
                   == DRing::DataTransferError::success);

    CPPUNIT_ASSERT(info.lastEvent == DRing::DataTransferEventCode::finished);
    CPPUNIT_ASSERT(info.bytesProgress == 64000);
    CPPUNIT_ASSERT(info.totalSize == 64000);

    // TODO FIX ME. The ICE take some time to stop and it doesn't seems to like
    // when stopping the daemon and removing the accounts to soon.
    std::remove(sendPath.c_str());
    std::remove(recvPath.c_str());
}

void
FileTransferTest::testMultipleFileTransfer()
{
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
    std::string finalId;
    // Watch signals
    confHandlers.insert(DRing::exportable_callback<DRing::DataTransferSignal::DataTransferEvent>(
        [&](const std::string& accountId,
            const std::string&,
            const std::string&,
            const std::string& fileId,
            int code) {
            if (accountId == bobId
                && code == static_cast<int>(DRing::DataTransferEventCode::wait_host_acceptance)) {
                transferWaiting = true;
                finalId = fileId;
                cv.notify_one();
            } else if (accountId == aliceId
                       && code == static_cast<int>(DRing::DataTransferEventCode::finished)) {
                transferFinished = true;
                finalId = fileId;
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);

    // Create file to send
    std::ofstream sendFile(sendPath);
    CPPUNIT_ASSERT(sendFile.is_open());
    sendFile << std::string(64000, 'A');
    sendFile.close();
    auto sendPath2 = std::filesystem::current_path().u8string() + DIR_SEPARATOR_CH + "SEND2";
    std::ofstream sendFile2(sendPath2);
    CPPUNIT_ASSERT(sendFile2.is_open());
    sendFile2 << std::string(64000, 'B');
    sendFile2.close();

    // Send first File
    DRing::DataTransferInfo info;
    uint64_t id;
    info.accountId = aliceAccount->getAccountID();
    info.peer = bobUri;
    info.path = sendPath;
    info.displayName = "SEND";
    info.bytesProgress = 0;
    CPPUNIT_ASSERT(DRing::sendFileLegacy(info, id) == DRing::DataTransferError::success);

    cv.wait_for(lk, std::chrono::seconds(30));
    CPPUNIT_ASSERT(transferWaiting);
    transferWaiting = false;

    CPPUNIT_ASSERT(DRing::acceptFileTransfer(bobId, finalId, recvPath)
                   == DRing::DataTransferError::success);

    // Wait 2 times, both sides will got a finished status
    cv.wait_for(lk, std::chrono::seconds(30));
    cv.wait_for(lk, std::chrono::seconds(30));
    CPPUNIT_ASSERT(transferFinished);

    CPPUNIT_ASSERT(compare(info.path, recvPath));

    // Send File
    DRing::DataTransferInfo info2;
    info2.accountId = aliceAccount->getAccountID();
    info2.peer = bobUri;
    info2.path = sendPath2;
    info2.displayName = "SEND2";
    info2.bytesProgress = 0;
    CPPUNIT_ASSERT(DRing::sendFileLegacy(info2, id) == DRing::DataTransferError::success);

    cv.wait_for(lk, std::chrono::seconds(30));
    CPPUNIT_ASSERT(transferWaiting);

    CPPUNIT_ASSERT(DRing::acceptFileTransfer(bobId, finalId, recv2Path)
                   == DRing::DataTransferError::success);

    // Wait 2 times, both sides will got a finished status
    cv.wait_for(lk, std::chrono::seconds(30));
    cv.wait_for(lk, std::chrono::seconds(30));
    CPPUNIT_ASSERT(transferFinished);

    CPPUNIT_ASSERT(compare(info2.path, recv2Path));

    // TODO FIX ME. The ICE take some time to stop and it doesn't seems to like
    // when stopping the daemon and removing the accounts to soon.
    std::remove(sendPath.c_str());
    std::remove(sendPath2.c_str());
    std::remove(recvPath.c_str());
    std::remove(recv2Path.c_str());
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
    Manager::instance().sendRegister(carlaId, true);
    wait_for_announcement_of(carlaId);

    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    auto requestReceived = 0;
    auto conversationReady = 0;
    auto memberJoined = 0;
    std::string tidBob, tidCarla, iidBob, iidCarla;
    std::string hostAcceptanceBob = {}, hostAcceptanceCarla = {};
    std::vector<std::string> peerAcceptance = {}, finished = {};
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> message) {
            if (message["type"] == "application/data-transfer+json") {
                if (accountId == bobId) {
                    tidBob = message["fileId"];
                    iidBob = message["id"];
                } else if (accountId == carlaId) {
                    tidCarla = message["fileId"];
                    iidCarla = message["id"];
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
        [&](const std::string& accountId,
            const std::string& conversationId,
            const std::string&,
            const std::string& fileId,
            int code) {
            if (conversationId.empty())
                return;
            if (code == static_cast<int>(DRing::DataTransferEventCode::wait_host_acceptance)) {
                if (accountId == bobId)
                    hostAcceptanceBob = fileId;
                else if (accountId == carlaId)
                    hostAcceptanceCarla = fileId;
                cv.notify_one();
            } else if (code
                       == static_cast<int>(DRing::DataTransferEventCode::wait_peer_acceptance)) {
                peerAcceptance.emplace_back(fileId);
                cv.notify_one();
            } else if (code == static_cast<int>(DRing::DataTransferEventCode::finished)) {
                finished.emplace_back(fileId);
                cv.notify_one();
            }
        }));
    DRing::registerSignalHandlers(confHandlers);

    auto convId = DRing::startConversation(aliceId);

    DRing::addConversationMember(aliceId, convId, bobUri);
    DRing::addConversationMember(aliceId, convId, carlaUri);
    cv.wait_for(lk, std::chrono::seconds(60), [&]() { return requestReceived == 2; });

    DRing::acceptConversationRequest(bobId, convId);
    DRing::acceptConversationRequest(carlaId, convId);
    cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return conversationReady == 3 && memberJoined == 2;
    });

    // Send file
    std::ofstream sendFile(sendPath);
    CPPUNIT_ASSERT(sendFile.is_open());
    sendFile << std::string(64000, 'A');
    sendFile.close();

    DRing::sendFile(aliceId, convId, sendPath, "SEND", "");

    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(45), [&]() {
        return !tidBob.empty() && !tidCarla.empty();
    }));

    DRing::downloadFile(bobId, convId, iidBob, tidBob, recvPath);
    DRing::downloadFile(carlaId, convId, iidCarla, tidCarla, recv2Path);

    CPPUNIT_ASSERT(
        cv.wait_for(lk, std::chrono::seconds(45), [&]() { return finished.size() == 3; }));

    std::remove(sendPath.c_str());
    std::remove(recvPath.c_str());
    std::remove(recv2Path.c_str());

    DRing::unregisterSignalHandlers();
}

void
FileTransferTest::testFileTransferInConversation()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    auto convId = DRing::startConversation(aliceId);

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool requestReceived = false;
    bool conversationReady = false;
    bool bobJoined = false;
    std::string tidBob, iidBob;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> message) {
            if (message["type"] == "application/data-transfer+json") {
                if (accountId == bobId) {
                    tidBob = message["fileId"];
                    iidBob = message["id"];
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
            const std::string&,
            const std::string&,
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

    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));

    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return conversationReady && bobJoined;
    }));

    // Create file to send
    std::ofstream sendFile(sendPath);
    CPPUNIT_ASSERT(sendFile.is_open());
    sendFile << std::string(64000, 'A');
    sendFile.close();

    DRing::sendFile(aliceId, convId, sendPath, "SEND", "");

    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return !tidBob.empty(); }));

    transferAFinished = false;
    transferBFinished = false;
    DRing::downloadFile(bobId, convId, iidBob, tidBob, recvPath);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return transferAFinished && transferBFinished;
    }));

    std::remove(sendPath.c_str());
    std::remove(recvPath.c_str());
    DRing::unregisterSignalHandlers();
    std::this_thread::sleep_for(std::chrono::seconds(5));
}

void
FileTransferTest::testVcfFileTransferInConversation()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    auto convId = DRing::startConversation(aliceId);

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool requestReceived = false;
    bool conversationReady = false;
    bool bobJoined = false;
    std::string tidBob, iidBob;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> message) {
            if (message["type"] == "application/data-transfer+json") {
                if (accountId == bobId) {
                    tidBob = message["fileId"];
                    iidBob = message["id"];
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
            const std::string&,
            const std::string&,
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

    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));

    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return conversationReady && bobJoined;
    }));

    // Create file to send
    std::ofstream sendFile(sendPath);
    CPPUNIT_ASSERT(sendFile.is_open());
    sendFile << std::string(64000, 'A');
    sendFile.close();

    DRing::sendFile(aliceId, convId, sendPath, "SEND", "");

    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return !tidBob.empty(); }));

    transferAFinished = false;
    transferBFinished = false;
    DRing::downloadFile(bobId, convId, iidBob, tidBob, recvPath);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return transferAFinished && transferBFinished;
    }));

    std::remove(sendPath.c_str());
    std::remove(recvPath.c_str());
    DRing::unregisterSignalHandlers();
    std::this_thread::sleep_for(std::chrono::seconds(5));
}

void
FileTransferTest::testBadSha3sumOut()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    auto convId = DRing::startConversation(aliceId);

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool requestReceived = false;
    bool conversationReady = false;
    bool memberJoin = false;
    std::string mid = {}, iid;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> message) {
            if (accountId == bobId) {
                if (message["type"] == "application/data-transfer+json") {
                    mid = message["fileId"];
                    iid = message["id"];
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
            const std::string&,
            const std::string&,
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

    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));

    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return conversationReady && memberJoin;
    }));

    // Create file to send
    std::ofstream sendFile(sendPath);
    CPPUNIT_ASSERT(sendFile.is_open());
    sendFile << std::string(64000, 'A');
    sendFile.close();

    DRing::sendFile(aliceId, convId, sendPath, "SEND", "");

    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return !mid.empty(); }));

    // modifiy file
    sendFile = std::ofstream(sendPath);
    CPPUNIT_ASSERT(sendFile.is_open());
    sendFile << std::string(64000, 'B');
    sendFile.close();

    transferAFinished = false;
    transferBFinished = false;
    DRing::downloadFile(bobId, convId, iid, mid, recvPath);

    // The file transfer will not be sent as modified
    CPPUNIT_ASSERT(!cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return transferAFinished || transferBFinished;
    }));

    std::remove(sendPath.c_str());
    DRing::unregisterSignalHandlers();
}

void
FileTransferTest::testBadSha3sumIn()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    auto convId = DRing::startConversation(aliceId);

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool requestReceived = false;
    bool conversationReady = false;
    bool memberJoin = false;
    std::string mid = {}, iid;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> message) {
            if (accountId == bobId) {
                if (message["type"] == "application/data-transfer+json") {
                    mid = message["fileId"];
                    iid = message["id"];
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
            const std::string&,
            const std::string&,
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

    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));

    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&]() {
        return conversationReady && memberJoin;
    }));

    // Create file to send
    std::ofstream sendFile(sendPath);
    CPPUNIT_ASSERT(sendFile.is_open());
    sendFile << std::string(64000, 'A');
    sendFile.close();

    aliceAccount->noSha3sumVerification(true);
    DRing::sendFile(aliceId, convId, sendPath, "SEND", "");
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return !mid.empty(); }));

    // modifiy file
    sendFile = std::ofstream(sendPath);
    CPPUNIT_ASSERT(sendFile.is_open());
    // Avoid ASAN error on big alloc   sendFile << std::string("B", 64000);
    sendFile << std::string(64000, 'B');
    sendFile.close();

    transferAFinished = false;
    transferBFinished = false;
    DRing::downloadFile(bobId, convId, iid, mid, recvPath);

    // The file transfer will be sent but refused by bob
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return transferAFinished; }));
    CPPUNIT_ASSERT(!cv.wait_for(lk, std::chrono::seconds(30), [&]() { return transferBFinished; }));

    std::remove(sendPath.c_str());
    DRing::unregisterSignalHandlers();
}

void
FileTransferTest::testAskToMultipleParticipants()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto carlaUri = carlaAccount->getUsername();
    auto convId = DRing::startConversation(aliceId);

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool requestReceived = false;
    bool conversationReady = false;
    bool memberJoin = false;
    std::string bobTid, carlaTid, iidBob, iidCarla;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> message) {
            if (message["type"] == "application/data-transfer+json") {
                if (accountId == bobId) {
                    bobTid = message["fileId"];
                    iidBob = message["id"];
                } else if (accountId == carlaId) {
                    carlaTid = message["fileId"];
                    iidCarla = message["id"];
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
            const std::string&,
            const std::string&,
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

    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));

    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&]() {
        return conversationReady && memberJoin;
    }));

    requestReceived = false;
    conversationReady = false;
    memberJoin = false;

    DRing::addConversationMember(aliceId, convId, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));

    DRing::acceptConversationRequest(carlaId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(60), [&]() {
        return conversationReady && memberJoin;
    }));

    // Create file to send
    std::ofstream sendFile(sendPath);
    CPPUNIT_ASSERT(sendFile.is_open());
    sendFile << std::string(64000, 'A');
    sendFile.close();

    DRing::sendFile(aliceId, convId, sendPath, "SEND", "");

    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return !bobTid.empty() && !carlaTid.empty();
    }));

    transferCFinished = false;
    DRing::downloadFile(carlaId, convId, iidCarla, carlaTid, recv2Path);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return transferCFinished; }));
    CPPUNIT_ASSERT(fileutils::isFile(recv2Path));

    transferBFinished = false;
    DRing::downloadFile(bobId, convId, iidBob, bobTid, recvPath);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return transferBFinished; }));
    CPPUNIT_ASSERT(fileutils::isFile(recvPath));

    std::remove(sendPath.c_str());
    std::remove(recvPath.c_str());
    std::remove(recv2Path.c_str());
    DRing::unregisterSignalHandlers();
}

void
FileTransferTest::testCancelInTransfer()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    auto convId = DRing::startConversation(aliceId);

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool requestReceived = false;
    bool conversationReady = false;
    bool bobJoined = false;
    std::string tidBob, iidBob;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> message) {
            if (message["type"] == "application/data-transfer+json") {
                if (accountId == bobId) {
                    iidBob = message["id"];
                    tidBob = message["fileId"];
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
    bool transferBOngoing = false, transferBCancelled = false;
    // Watch signals
    confHandlers.insert(DRing::exportable_callback<DRing::DataTransferSignal::DataTransferEvent>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            const std::string&,
            const std::string&,
            int code) {
            if (code == static_cast<int>(DRing::DataTransferEventCode::ongoing)
                && conversationId == convId) {
                if (accountId == bobId)
                    transferBOngoing = true;
            } else if (code > static_cast<int>(DRing::DataTransferEventCode::finished)
                       && conversationId == convId) {
                if (accountId == bobId)
                    transferBCancelled = true;
            }
            cv.notify_one();
        }));
    DRing::registerSignalHandlers(confHandlers);

    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));

    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return conversationReady && bobJoined;
    }));

    // Create file to send
    std::ofstream sendFile(sendPath);
    CPPUNIT_ASSERT(sendFile.is_open());
    sendFile << std::string(640000, 'A');
    sendFile.close();

    DRing::sendFile(aliceId, convId, sendPath, "SEND", "");

    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return !tidBob.empty(); }));

    transferBOngoing = false;
    CPPUNIT_ASSERT(DRing::downloadFile(bobId, convId, iidBob, tidBob, recvPath));
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return transferBOngoing; }));
    transferBCancelled = false;
    DRing::cancelDataTransfer(bobId, convId, tidBob);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return transferBCancelled; }));
    CPPUNIT_ASSERT(!fileutils::isFile(recvPath));
    CPPUNIT_ASSERT(!bobAccount->dataTransfer(convId)->isWaiting(tidBob));

    std::remove(sendPath.c_str());
    DRing::unregisterSignalHandlers();
}

void
FileTransferTest::testTransferInfo()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    auto convId = DRing::startConversation(aliceId);

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    bool requestReceived = false;
    bool conversationReady = false;
    bool bobJoined = false;
    std::string tidBob, iidBob;
    confHandlers.insert(DRing::exportable_callback<DRing::ConversationSignal::MessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> message) {
            if (message["type"] == "application/data-transfer+json") {
                if (accountId == bobId) {
                    tidBob = message["fileId"];
                    iidBob = message["id"];
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
            const std::string&,
            const std::string&,
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

    DRing::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return requestReceived; }));

    DRing::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return conversationReady && bobJoined;
    }));

    // Create file to send
    std::ofstream sendFile(sendPath);
    CPPUNIT_ASSERT(sendFile.is_open());
    sendFile << std::string(64000, 'A');
    sendFile.close();

    DRing::sendFile(aliceId, convId, sendPath, "SEND", "");
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() { return !tidBob.empty(); }));

    int64_t totalSize, bytesProgress;
    std::string path;
    CPPUNIT_ASSERT(DRing::fileTransferInfo(bobId, convId, tidBob, path, totalSize, bytesProgress)
                   == DRing::DataTransferError::invalid_argument);
    CPPUNIT_ASSERT(bytesProgress == 0);
    CPPUNIT_ASSERT(!fileutils::isFile(path));
    // No check for total as not started

    transferAFinished = false;
    transferBFinished = false;
    DRing::downloadFile(bobId, convId, iidBob, tidBob, recvPath);
    CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(30), [&]() {
        return transferAFinished && transferBFinished;
    }));
    CPPUNIT_ASSERT(DRing::fileTransferInfo(bobId, convId, tidBob, path, totalSize, bytesProgress)
                   == DRing::DataTransferError::success);

    CPPUNIT_ASSERT(bytesProgress == 64000);
    CPPUNIT_ASSERT(totalSize == 64000);
    CPPUNIT_ASSERT(fileutils::isFile(path));

    std::remove(sendPath.c_str());
    std::remove(recvPath.c_str());
    DRing::unregisterSignalHandlers();
    std::this_thread::sleep_for(std::chrono::seconds(5));
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::FileTransferTest::name())
