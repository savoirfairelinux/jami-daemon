/*
 *  Copyright (C) 2004-2025 Savoir-faire Linux Inc.
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
#include "../../test_runner.h"
#include "jami.h"
#include "data_transfer.h"
#include "jami/datatransfer_interface.h"
#include "account_const.h"
#include "common.h"

#include <dhtnet/connectionmanager.h>

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <condition_variable>
#include <string>
#include <filesystem>

using namespace std::literals::chrono_literals;
using namespace libjami::Account;
using namespace std::literals::chrono_literals;

namespace jami {
namespace test {

struct UserData {
    std::string conversationId;
    bool removed {false};
    bool requestReceived {false};
    bool registered {false};
    bool stopped {false};
    bool deviceAnnounced {false};
    int code {0};
    std::vector<libjami::SwarmMessage> messages;
    std::vector<libjami::SwarmMessage> messagesUpdated;
};

class FileTransferTest : public CppUnit::TestFixture
{
public:
    FileTransferTest()
    {
        // Init daemon
        libjami::init(
            libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    }
    ~FileTransferTest() { libjami::fini(); }
    static std::string name() { return "Call"; }
    bool compare(const std::string& fileA, const std::string& fileB) const;
    void setUp();
    void tearDown();

    std::string aliceId;
    UserData aliceData;
    std::string bobId;
    UserData bobData;
    std::string carlaId;
    UserData carlaData;

    std::filesystem::path sendPath {std::filesystem::current_path() / "SEND"};
    std::filesystem::path recvPath {std::filesystem::current_path() / "RECV"};
    std::filesystem::path recv2Path {std::filesystem::current_path() / "RECV2"};

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;

    void connectSignals();

private:
    void testConversationFileTransfer();
    void testFileTransferInConversation();
    void testVcfFileTransferInConversation();
    void testBadSha3sumOut();
    void testBadSha3sumIn();
    void testAskToMultipleParticipants();
    void testCancelInTransfer();
    void testResumeTransferAfterInterruption();
    void testDontDownloadExistingFile();
    void testTransferInfo();
    void testRemoveHardLink();
    void testTooLarge();
    void testDeleteFile();

    CPPUNIT_TEST_SUITE(FileTransferTest);
    CPPUNIT_TEST(testConversationFileTransfer);
    CPPUNIT_TEST(testFileTransferInConversation);
    CPPUNIT_TEST(testVcfFileTransferInConversation);
    CPPUNIT_TEST(testBadSha3sumOut);
    CPPUNIT_TEST(testBadSha3sumIn);
    CPPUNIT_TEST(testAskToMultipleParticipants);
    CPPUNIT_TEST(testCancelInTransfer);
    CPPUNIT_TEST(testResumeTransferAfterInterruption);
    CPPUNIT_TEST(testDontDownloadExistingFile);
    CPPUNIT_TEST(testTransferInfo);
    CPPUNIT_TEST(testRemoveHardLink);
    CPPUNIT_TEST(testTooLarge);
    CPPUNIT_TEST(testDeleteFile);
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
    aliceData = {};
    bobData = {};
    carlaData = {};
}

void
FileTransferTest::tearDown()
{
    std::filesystem::remove(sendPath);
    std::filesystem::remove(recvPath);
    std::filesystem::remove(recv2Path);
    wait_for_removal_of({aliceId, bobId, carlaId});
}

void
FileTransferTest::connectSignals()
{
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& conversationId) {
            if (accountId == aliceId) {
                aliceData.conversationId = conversationId;
            } else if (accountId == bobId) {
                bobData.conversationId = conversationId;
            } else if (accountId == carlaId) {
                carlaData.conversationId = conversationId;
            }
            cv.notify_one();
        }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
            [&](const std::string& accountId,
                const std::string& /* conversationId */,
                std::map<std::string, std::string> /*metadatas*/) {
                if (accountId == aliceId) {
                    aliceData.requestReceived = true;
                } else if (accountId == bobId) {
                    bobData.requestReceived = true;
                } else if (accountId == carlaId) {
                    carlaData.requestReceived = true;
                }
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::SwarmMessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            libjami::SwarmMessage message) {
            std::unique_lock<std::mutex> lk {mtx};
            if (accountId == aliceId) {
                aliceData.messages.emplace_back(message);
            } else if (accountId == bobId) {
                bobData.messages.emplace_back(message);
            } else if (accountId == carlaId) {
                carlaData.messages.emplace_back(message);
            }
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::SwarmMessageUpdated>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            libjami::SwarmMessage message) {
            if (accountId == aliceId) {
                aliceData.messagesUpdated.emplace_back(message);
            } else if (accountId == bobId) {
                bobData.messagesUpdated.emplace_back(message);
            } else if (accountId == carlaId) {
                carlaData.messagesUpdated.emplace_back(message);
            }
            cv.notify_one();
        }));
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::ConversationRemoved>(
            [&](const std::string& accountId, const std::string&) {
                if (accountId == aliceId)
                    aliceData.removed = true;
                else if (accountId == bobId)
                    bobData.removed = true;
                cv.notify_one();
            }));
    confHandlers.insert(libjami::exportable_callback<libjami::DataTransferSignal::DataTransferEvent>(
        [&](const std::string& accountId,
            const std::string& conversationId,
            const std::string&,
            const std::string& fileId,
            int code) {
            if (conversationId.empty())
                return;
            if (accountId == aliceId)
                aliceData.code = code;
            else if (accountId == bobId)
                bobData.code = code;
            else if (accountId == carlaId)
                carlaData.code = code;
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);
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

    connectSignals();

    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    libjami::addConversationMember(aliceId, convId, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() { return bobData.requestReceived && carlaData.requestReceived; }));

    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));
    aliceMsgSize = aliceData.messages.size();
    auto bobMsgSize = bobData.messages.size();
    libjami::acceptConversationRequest(carlaId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size() && bobMsgSize + 1 == bobData.messages.size(); }));

    // Send file
    std::ofstream sendFile(sendPath);
    CPPUNIT_ASSERT(sendFile.is_open());
    sendFile << std::string(64000, 'A');
    sendFile.close();

    bobMsgSize = bobData.messages.size();
    auto carlaMsgSize = carlaData.messages.size();
    libjami::sendFile(aliceId, convId, sendPath, "SEND", "");

    CPPUNIT_ASSERT(cv.wait_for(lk, 45s, [&]() { return bobData.messages.size() == bobMsgSize + 1 && carlaData.messages.size() == carlaMsgSize + 1; }));
    auto id = bobData.messages.rbegin()->id;
    auto fileId = bobData.messages.rbegin()->body["fileId"];

    libjami::downloadFile(bobId, convId, id, fileId, recvPath);
    libjami::downloadFile(carlaId, convId, id, fileId, recv2Path);

    CPPUNIT_ASSERT(cv.wait_for(lk, 45s, [&]() { return carlaData.code == static_cast<int>(libjami::DataTransferEventCode::finished) && bobData.code == static_cast<int>(libjami::DataTransferEventCode::finished); }));
}

void
FileTransferTest::testFileTransferInConversation()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    connectSignals();

    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));

    // Create file to send
    std::ofstream sendFile(sendPath);
    CPPUNIT_ASSERT(sendFile.is_open());
    sendFile << std::string(64000, 'A');
    sendFile.close();

    auto bobMsgSize = bobData.messages.size();
    libjami::sendFile(aliceId, convId, sendPath, "SEND", "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobMsgSize  + 1== bobData.messages.size(); }));

    auto id = bobData.messages.rbegin()->id;
    auto fileId = bobData.messages.rbegin()->body["fileId"];
    libjami::downloadFile(bobId, convId, id, fileId, recvPath);

    CPPUNIT_ASSERT(cv.wait_for(lk, 45s, [&]() { return aliceData.code == static_cast<int>(libjami::DataTransferEventCode::finished) && bobData.code == static_cast<int>(libjami::DataTransferEventCode::finished); }));
}

void
FileTransferTest::testVcfFileTransferInConversation()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    connectSignals();
    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));

    // Create file to send
    std::ofstream sendFile(sendPath);
    CPPUNIT_ASSERT(sendFile.is_open());
    sendFile << std::string(64000, 'A');
    sendFile.close();

    auto bobMsgSize = bobData.messages.size();
    libjami::sendFile(aliceId, convId, sendPath, "SEND", "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobMsgSize + 1 == bobData.messages.size(); }));

    auto id = bobData.messages.rbegin()->id;
    auto fileId = bobData.messages.rbegin()->body["fileId"];
    libjami::downloadFile(bobId, convId, id, fileId, recvPath);

    CPPUNIT_ASSERT(cv.wait_for(lk, 45s, [&]() { return aliceData.code == static_cast<int>(libjami::DataTransferEventCode::finished) && bobData.code == static_cast<int>(libjami::DataTransferEventCode::finished); }));
}

void
FileTransferTest::testBadSha3sumOut()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    connectSignals();
    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));

    // Create file to send
    std::ofstream sendFile(sendPath);
    CPPUNIT_ASSERT(sendFile.is_open());
    sendFile << std::string(64000, 'A');
    sendFile.close();

    auto bobMsgSize = bobData.messages.size();
    libjami::sendFile(aliceId, convId, sendPath, "SEND", "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobMsgSize + 1 == bobData.messages.size(); }));

    // modifiy file
    sendFile = std::ofstream(sendPath, std::ios::trunc);
    CPPUNIT_ASSERT(sendFile.is_open());
    sendFile << std::string(64000, 'B');
    sendFile.close();

    auto id = bobData.messages.rbegin()->id;
    auto fileId = bobData.messages.rbegin()->body["fileId"];
    libjami::downloadFile(bobId, convId, id, fileId, recvPath);

    CPPUNIT_ASSERT(!cv.wait_for(lk, 45s, [&]() { return aliceData.code == static_cast<int>(libjami::DataTransferEventCode::finished) || bobData.code == static_cast<int>(libjami::DataTransferEventCode::finished); }));
}

void
FileTransferTest::testBadSha3sumIn()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    connectSignals();
    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));

    // Create file to send
    std::ofstream sendFile(sendPath);
    CPPUNIT_ASSERT(sendFile.is_open());
    sendFile << std::string(64000, 'A');
    sendFile.close();

    aliceAccount->noSha3sumVerification(true);
    auto bobMsgSize = bobData.messages.size();
    libjami::sendFile(aliceId, convId, sendPath, "SEND", "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobMsgSize + 1 == bobData.messages.size(); }));

    // modifiy file
    sendFile = std::ofstream(sendPath);
    CPPUNIT_ASSERT(sendFile.is_open());
    // Avoid ASAN error on big alloc   sendFile << std::string("B", 64000);
    sendFile << std::string(64000, 'B');
    sendFile.close();

    auto id = bobData.messages.rbegin()->id;
    auto fileId = bobData.messages.rbegin()->body["fileId"];
    libjami::downloadFile(bobId, convId, id, fileId, recvPath);

    // The file transfer will be sent but refused by bob
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceData.code == static_cast<int>(libjami::DataTransferEventCode::finished); }));
    CPPUNIT_ASSERT(!cv.wait_for(lk, 30s, [&]() { return bobData.code == static_cast<int>(libjami::DataTransferEventCode::finished); }));

    std::filesystem::remove(sendPath);
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
    connectSignals();
    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    libjami::addConversationMember(aliceId, convId, carlaUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() { return bobData.requestReceived && carlaData.requestReceived; }));

    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));
    aliceMsgSize = aliceData.messages.size();
    auto bobMsgSize = bobData.messages.size();
    libjami::acceptConversationRequest(carlaId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size() && bobMsgSize + 1 == bobData.messages.size(); }));

    // Create file to send
    std::ofstream sendFile(sendPath);
    CPPUNIT_ASSERT(sendFile.is_open());
    sendFile << std::string(64000, 'A');
    sendFile.close();

    bobMsgSize = bobData.messages.size();
    auto carlaMsgSize = carlaData.messages.size();
    libjami::sendFile(aliceId, convId, sendPath, "SEND", "");

    CPPUNIT_ASSERT(cv.wait_for(lk, 45s, [&]() { return bobData.messages.size() == bobMsgSize + 1 && carlaData.messages.size() == carlaMsgSize + 1; }));
    auto id = bobData.messages.rbegin()->id;
    auto fileId = bobData.messages.rbegin()->body["fileId"];

    libjami::downloadFile(carlaId, convId, id, fileId, recv2Path);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return carlaData.code == static_cast<int>(libjami::DataTransferEventCode::finished); }));
    CPPUNIT_ASSERT(dhtnet::fileutils::isFile(recv2Path));

    libjami::downloadFile(bobId, convId, id, fileId, recvPath);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.code == static_cast<int>(libjami::DataTransferEventCode::finished); }));
    CPPUNIT_ASSERT(dhtnet::fileutils::isFile(recvPath));
}

void
FileTransferTest::testCancelInTransfer()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    connectSignals();
    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));

    // Create file to send
    std::ofstream sendFile(sendPath);
    CPPUNIT_ASSERT(sendFile.is_open());
    sendFile << std::string(640000, 'A');
    sendFile.close();

    auto bobMsgSize = bobData.messages.size();
    libjami::sendFile(aliceId, convId, sendPath, "SEND", "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobMsgSize + 1 == bobData.messages.size(); }));
    auto id = bobData.messages.rbegin()->id;
    auto fileId = bobData.messages.rbegin()->body["fileId"];

    libjami::downloadFile(bobId, convId, id, fileId, recvPath);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.code == static_cast<int>(libjami::DataTransferEventCode::ongoing); }));

    libjami::cancelDataTransfer(bobId, convId, fileId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.code == static_cast<int>(libjami::DataTransferEventCode::closed_by_peer); }));
    CPPUNIT_ASSERT(!dhtnet::fileutils::isFile(recvPath));
    CPPUNIT_ASSERT(!bobAccount->dataTransfer(convId)->isWaiting(fileId));
}

void
FileTransferTest::testResumeTransferAfterInterruption()
{
    // Create conversation
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    connectSignals();
    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));

    // Create file to send
    constexpr int64_t totalSize = 12800000;
    std::ofstream sendFile(sendPath);
    CPPUNIT_ASSERT(sendFile.is_open());
    sendFile << std::string(totalSize, 'A');
    sendFile.close();

    // Send file info
    auto bobMsgSize = bobData.messages.size();
    libjami::sendFile(aliceId, convId, sendPath, "SEND", "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobMsgSize + 1 == bobData.messages.size(); }));
    auto id = bobData.messages.rbegin()->id;
    auto fileId = bobData.messages.rbegin()->body["fileId"];

    libjami::downloadFile(bobId, convId, id, fileId, recvPath);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.code == static_cast<int>(libjami::DataTransferEventCode::ongoing); }));

    Manager::instance().sendRegister(aliceId, false);

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.code == static_cast<int>(libjami::DataTransferEventCode::closed_by_host); }));

    int64_t receivedSize = fileutils::size(recvPath + std::string(".tmp"));
    CPPUNIT_ASSERT(receivedSize < totalSize);
    CPPUNIT_ASSERT(0 < receivedSize);
    CPPUNIT_ASSERT(bobAccount->dataTransfer(convId)->isWaiting(fileId));

    Manager::instance().sendRegister(aliceId, true);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.code == static_cast<int>(libjami::DataTransferEventCode::finished); }));
    CPPUNIT_ASSERT(!bobAccount->dataTransfer(convId)->isWaiting(fileId));
}

void
FileTransferTest::testDontDownloadExistingFile()
{
    // Create conversation
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    connectSignals();
    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));

    // Create file to send
    constexpr int64_t totalSize = 128000;
    std::ofstream sendFile(sendPath);
    CPPUNIT_ASSERT(sendFile.is_open());
    sendFile << std::string(totalSize, 'A');
    sendFile.close();

    std::filesystem::copy(sendPath, recvPath);

    // Send file info
    auto bobMsgSize = bobData.messages.size();
    libjami::sendFile(aliceId, convId, sendPath, "SEND", "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobMsgSize + 1 == bobData.messages.size(); }));
    auto id = bobData.messages.rbegin()->id;
    auto fileId = bobData.messages.rbegin()->body["fileId"];

    libjami::downloadFile(bobId, convId, id, fileId, recvPath);
    CPPUNIT_ASSERT(!cv.wait_for(lk, 10s, [&]() { return bobData.code > 0; }));
    CPPUNIT_ASSERT(!bobAccount->dataTransfer(convId)->isWaiting(fileId));
    CPPUNIT_ASSERT(fileutils::size(recvPath) == totalSize);
}

void
FileTransferTest::testTransferInfo()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto aliceUri = aliceAccount->getUsername();
    connectSignals();
    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));

    // Create file to send
    std::ofstream sendFile(sendPath);
    CPPUNIT_ASSERT(sendFile.is_open());
    sendFile << std::string(640000, 'A');
    sendFile.close();

    auto bobMsgSize = bobData.messages.size();
    libjami::sendFile(aliceId, convId, sendPath, "SEND", "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobMsgSize + 1 == bobData.messages.size(); }));
    auto id = bobData.messages.rbegin()->id;
    auto fileId = bobData.messages.rbegin()->body["fileId"];

    int64_t totalSize, bytesProgress;
    std::string path;
    CPPUNIT_ASSERT(libjami::fileTransferInfo(bobId, convId, fileId, path, totalSize, bytesProgress)
                   == libjami::DataTransferError::invalid_argument);
    CPPUNIT_ASSERT(bytesProgress == 0);
    CPPUNIT_ASSERT(!std::filesystem::is_regular_file(path));
    // No check for total as not started

    libjami::downloadFile(bobId, convId, id, fileId, recvPath);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.code == static_cast<int>(libjami::DataTransferEventCode::finished); }));
    CPPUNIT_ASSERT(libjami::fileTransferInfo(bobId, convId, fileId, path, totalSize, bytesProgress)
                   == libjami::DataTransferError::success);
    CPPUNIT_ASSERT(bytesProgress == 640000);
    CPPUNIT_ASSERT(totalSize == 640000);
    CPPUNIT_ASSERT(dhtnet::fileutils::isFile(path));
}

void
FileTransferTest::testRemoveHardLink()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    connectSignals();
    auto convId = libjami::startConversation(aliceId);

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !aliceData.conversationId.empty(); }));

    // Send file
    std::ofstream sendFile(sendPath);
    CPPUNIT_ASSERT(sendFile.is_open());
    sendFile << std::string(64000, 'A');
    sendFile.close();

    libjami::sendFile(aliceId, convId, sendPath, std::filesystem::absolute("SEND"), "");

    auto aliceMsgSize = aliceData.messages.size();
    CPPUNIT_ASSERT(cv.wait_for(lk, 45s, [&]() { return aliceMsgSize + 1 == aliceData.messages.size(); }));

    CPPUNIT_ASSERT(libjami::removeConversation(aliceId, convId));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceData.removed; }));

    auto content = fileutils::loadTextFile(sendPath);
    CPPUNIT_ASSERT(content.find("AAA") != std::string::npos);
}

void
FileTransferTest::testTooLarge()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    connectSignals();
    auto convId = libjami::startConversation(aliceId);

    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));

    auto aliceMsgSize = aliceData.messages.size();
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() {
        return aliceMsgSize + 1 == aliceData.messages.size(); }));

    // Send file
    std::ofstream sendFile(sendPath);
    CPPUNIT_ASSERT(sendFile.is_open());
    sendFile << std::string(64000, 'A');
    sendFile.close();

    auto bobMsgSize = bobData.messages.size();
    libjami::sendFile(aliceId, convId, sendPath, "SEND", "");
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobMsgSize + 1 == bobData.messages.size(); }));
    auto id = bobData.messages.rbegin()->id;
    auto fileId = bobData.messages.rbegin()->body["fileId"];

    // Add some data for the reception. This will break the final shasum
    std::ofstream recvFile(recvPath + std::string(".tmp"));
    CPPUNIT_ASSERT(recvFile.is_open());
    recvFile << std::string(1000, 'B');
    recvFile.close();
    libjami::downloadFile(bobId, convId, id, fileId, recvPath);

    CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&]() { return bobData.code == static_cast<int>(libjami::DataTransferEventCode::closed_by_host); }));
    CPPUNIT_ASSERT(!dhtnet::fileutils::isFile(recvPath));
}

void
FileTransferTest::testDeleteFile()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto convId = libjami::startConversation(aliceId);

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool conversationReady = false;
    std::string iid, tid;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::SwarmMessageReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            libjami::SwarmMessage message) {
            if (message.type == "application/data-transfer+json") {
                if (accountId == aliceId) {
                    iid = message.id;
                    tid = message.body["tid"];
                }
            }
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [&](const std::string& accountId, const std::string& /* conversationId */) {
            if (accountId == bobId) {
                conversationReady = true;
                cv.notify_one();
            }
        }));
    bool messageUpdated = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::SwarmMessageUpdated>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            libjami::SwarmMessage message) {
            if (accountId == aliceId && message.type == "application/data-transfer+json" && message.body["tid"].empty()) {
                messageUpdated = true;
            }
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);

    // Create file to send
    std::ofstream sendFile(sendPath);
    CPPUNIT_ASSERT(sendFile.is_open());
    sendFile << std::string(64000, 'A');
    sendFile.close();

    libjami::sendFile(aliceId, convId, sendPath, "SEND", "");

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !iid.empty(); }));
    auto dataPath = fileutils::get_data_dir() / aliceId / "conversation_data" / convId;
    CPPUNIT_ASSERT(dhtnet::fileutils::isFile(dataPath / fmt::format("{}_{}", iid, tid)));

    // Delete file
    libjami::sendMessage(aliceId, convId, ""s, iid, 1);

    // Verify message is updated
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return messageUpdated; }));
    // Verify file is deleted
    CPPUNIT_ASSERT(!dhtnet::fileutils::isFile(dataPath / fmt::format("{}_{}", iid, tid)));

    libjami::unregisterSignalHandlers();
    std::this_thread::sleep_for(5s);
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::FileTransferTest::name())
