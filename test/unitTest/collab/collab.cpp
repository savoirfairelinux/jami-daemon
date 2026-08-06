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
#include "common.h"

#include "jami.h"
#include "account_const.h"
#include "fileutils.h"
#include "manager.h"
#include "jamidht/jamiaccount.h"

extern "C" {
#include <libyrs.h>
}

#include <condition_variable>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <vector>

using namespace std::string_literals;
using namespace std::literals::chrono_literals;
using namespace libjami::Account;

namespace jami {
namespace test {

/**
 * A client-side Y-CRDT replica, as an editor application would hold one.
 *
 * The daemon is a transport: it moves opaque updates around. These tests
 * therefore need to author real updates and read the resulting text back,
 * which is what this little yffi wrapper does. The shared type is a "text"
 * root branch, by convention of the tests only — the daemon never knows.
 */
class ClientReplica
{
public:
    ClientReplica()
        : doc_(ydoc_new())
    {}
    ~ClientReplica() { ydoc_destroy(doc_); }
    ClientReplica(const ClientReplica&) = delete;
    ClientReplica& operator=(const ClientReplica&) = delete;

    /// Insert text, returning the update to hand to the daemon.
    std::vector<uint8_t> insert(uint32_t index, const std::string& text)
    {
        auto before = stateVector();
        auto* txt = ytext(doc_, "text");
        auto* txn = ydoc_write_transaction(doc_, 0, nullptr);
        ytext_insert(txt, txn, index, text.c_str(), nullptr);
        ytransaction_commit(txn);
        return diffSince(before);
    }

    /// Merge an update received from the daemon.
    bool apply(const std::vector<uint8_t>& update)
    {
        if (update.empty())
            return false;
        auto* txn = ydoc_write_transaction(doc_, 0, nullptr);
        auto result = ytransaction_apply(txn,
                                         reinterpret_cast<const char*>(update.data()),
                                         static_cast<uint32_t>(update.size()));
        ytransaction_commit(txn);
        return result == 0;
    }

    /// Current content of the "text" branch.
    std::string text()
    {
        auto* txt = ytext(doc_, "text");
        auto* txn = ydoc_read_transaction(doc_);
        char* str = ytext_string(txt, txn);
        std::string out = str ? str : "";
        if (str)
            ystring_destroy(str);
        ytransaction_commit(txn);
        return out;
    }

private:
    std::vector<uint8_t> stateVector()
    {
        auto* txn = ydoc_read_transaction(doc_);
        uint32_t len = 0;
        char* sv = ytransaction_state_vector_v1(txn, &len);
        std::vector<uint8_t> out(sv, sv + len);
        ybinary_destroy(sv, len);
        ytransaction_commit(txn);
        return out;
    }

    std::vector<uint8_t> diffSince(const std::vector<uint8_t>& sv)
    {
        auto* txn = ydoc_read_transaction(doc_);
        uint32_t len = 0;
        char* diff = ytransaction_state_diff_v1(txn,
                                                reinterpret_cast<const char*>(sv.data()),
                                                static_cast<uint32_t>(sv.size()),
                                                &len);
        std::vector<uint8_t> out(diff, diff + len);
        ybinary_destroy(diff, len);
        ytransaction_commit(txn);
        return out;
    }

    YDoc* doc_;
};

struct UserData
{
    std::string conversationId;
    bool registered {false};
    bool stopped {false};
    bool requestReceived {false};
    bool conversationRemoved {false};
    bool deviceAnnounced {false};
    std::vector<libjami::SwarmMessage> messages;

    // Collaborative-editing signals, keyed by document id where relevant.
    std::map<std::string, std::vector<std::vector<uint8_t>>> docUpdates;
    std::map<std::string, std::vector<std::string>> awareness;
    std::map<std::string, int> participantLeft;
    std::map<std::string, std::string> renamedTo;
    std::map<std::string, int> removedEverywhere;
    std::map<std::string, int> removedLocally;
    std::map<std::string, std::vector<std::string>> attachmentsAdded;
};

class CollabTest : public CppUnit::TestFixture
{
public:
    ~CollabTest() { libjami::fini(); }
    static std::string name() { return "Collab"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    UserData aliceData;
    std::string bobId;
    UserData bobData;
    std::string bob2Id;
    UserData bob2Data;
    std::string carlaId;
    UserData carlaData;

    std::mutex mtx;
    std::condition_variable cv;

    void connectSignals();

    /// Alice starts a swarm and brings Bob in; returns the conversation id.
    std::string createConversationWithBob();
    /// Where the new design stores a document repository.
    std::filesystem::path docRepoPath(const std::string& accountId, const std::string& docId);
    /// The listing entry of one document, or empty if not listed.
    std::map<std::string, std::string> documentEntry(const std::string& accountId,
                                                     const std::string& convId,
                                                     const std::string& docId);
    /// Poll until @p pred is true or @p timeout elapsed. @p pred runs without
    /// the fixture lock so it may call daemon APIs freely; predicates reading
    /// fields written by the signal handlers must lock @c mtx themselves.
    bool poll(std::function<bool()> pred, std::chrono::seconds timeout = 30s);
    /// Wait until a checkpoint of @p docId is visible in @p accountId's history.
    bool waitForCheckpoint(const std::string& accountId,
                           const std::string& convId,
                           const std::string& docId,
                           size_t atLeast = 1);

private:
    void testCreateDocument();
    void testNoAutoCloneOnAnnouncement();
    void testOpenClonesAndJoins();
    void testClosedHolderConvergesViaCheckpoints();
    void testPerDeviceOptIn();
    void testRenameAdminOnly();
    void testAttachmentReplication();
    void testRemoveDocumentEverywhere();
    void testRemoveDocumentLocallyAndReopen();
    void testDocumentBanRefusesClone();
    void testParentConversationLeaveLeavesDocuments();
    void testRealtimeUpdatePropagation();
    void testAwareness();

    CPPUNIT_TEST_SUITE(CollabTest);
    CPPUNIT_TEST(testCreateDocument);
    CPPUNIT_TEST(testNoAutoCloneOnAnnouncement);
    CPPUNIT_TEST(testOpenClonesAndJoins);
    CPPUNIT_TEST(testClosedHolderConvergesViaCheckpoints);
    CPPUNIT_TEST(testPerDeviceOptIn);
    CPPUNIT_TEST(testRenameAdminOnly);
    CPPUNIT_TEST(testAttachmentReplication);
    CPPUNIT_TEST(testRemoveDocumentEverywhere);
    CPPUNIT_TEST(testRemoveDocumentLocallyAndReopen);
    CPPUNIT_TEST(testDocumentBanRefusesClone);
    CPPUNIT_TEST(testParentConversationLeaveLeavesDocuments);
    CPPUNIT_TEST(testRealtimeUpdatePropagation);
    CPPUNIT_TEST(testAwareness);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(CollabTest, CollabTest::name());

void
CollabTest::setUp()
{
    libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
    if (not Manager::instance().initialized)
        CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));

    auto actors = load_actors("actors/alice-bob-carla.yml");
    aliceId = actors["alice"];
    bobId = actors["bob"];
    carlaId = actors["carla"];

    aliceData = {};
    bobData = {};
    bob2Data = {};
    carlaData = {};

    Manager::instance().sendRegister(carlaId, false);
    wait_for_announcement_of({aliceId, bobId});
}

void
CollabTest::tearDown()
{
    auto bobArchive = std::filesystem::current_path().string() + "/bob.gz";
    std::remove(bobArchive.c_str());

    if (bob2Id.empty()) {
        wait_for_removal_of({aliceId, bobId, carlaId});
    } else {
        wait_for_removal_of({aliceId, bobId, carlaId, bob2Id});
    }
}

void
CollabTest::connectSignals()
{
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;

    auto dataFor = [&](const std::string& accountId) -> UserData* {
        if (accountId == aliceId)
            return &aliceData;
        if (accountId == bobId)
            return &bobData;
        if (accountId == bob2Id)
            return &bob2Data;
        if (accountId == carlaId)
            return &carlaData;
        return nullptr;
    };

    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::VolatileDetailsChanged>(
        [=, this](const std::string& accountId, const std::map<std::string, std::string>&) {
            // Query the daemon before taking the fixture lock, never under it.
            auto account = Manager::instance().getAccount<JamiAccount>(accountId);
            if (!account)
                return;
            auto details = account->getVolatileAccountDetails();
            auto daemonStatus = details[libjami::Account::ConfProperties::Registration::STATUS];
            std::lock_guard<std::mutex> lock(mtx);
            if (auto* data = dataFor(accountId)) {
                if (daemonStatus == "REGISTERED")
                    data->registered = true;
                else if (daemonStatus == "UNREGISTERED")
                    data->stopped = true;
                data->deviceAnnounced = details[libjami::Account::VolatileProperties::DEVICE_ANNOUNCED] == "true";
                cv.notify_one();
            }
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationReady>(
        [=, this](const std::string& accountId, const std::string& conversationId) {
            std::lock_guard<std::mutex> lock(mtx);
            if (auto* data = dataFor(accountId))
                data->conversationId = conversationId;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
        [=, this](const std::string& accountId,
                  const std::string& /*conversationId*/,
                  std::map<std::string, std::string> /*metadatas*/) {
            std::lock_guard<std::mutex> lock(mtx);
            if (auto* data = dataFor(accountId))
                data->requestReceived = true;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::SwarmMessageReceived>(
        [=, this](const std::string& accountId, const std::string& /*conversationId*/, libjami::SwarmMessage message) {
            std::lock_guard<std::mutex> lock(mtx);
            if (auto* data = dataFor(accountId))
                data->messages.emplace_back(message);
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationRemoved>(
        [=, this](const std::string& accountId, const std::string&) {
            std::lock_guard<std::mutex> lock(mtx);
            if (auto* data = dataFor(accountId))
                data->conversationRemoved = true;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::CollaborativeDocumentUpdate>(
        [=, this](const std::string& accountId,
                  const std::string& /*convId*/,
                  const std::string& documentId,
                  const std::vector<uint8_t>& update) {
            std::lock_guard<std::mutex> lock(mtx);
            if (auto* data = dataFor(accountId))
                data->docUpdates[documentId].emplace_back(update);
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::CollaborativeAwarenessChanged>(
        [=, this](const std::string& accountId,
                  const std::string& /*convId*/,
                  const std::string& documentId,
                  const std::string& /*peerId*/,
                  uint64_t /*clientId*/,
                  const std::string& state) {
            std::lock_guard<std::mutex> lock(mtx);
            if (auto* data = dataFor(accountId))
                data->awareness[documentId].emplace_back(state);
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::CollaborativeParticipantLeft>(
        [=, this](const std::string& accountId,
                  const std::string& /*convId*/,
                  const std::string& documentId,
                  const std::string& /*peerId*/,
                  uint64_t /*clientId*/) {
            std::lock_guard<std::mutex> lock(mtx);
            if (auto* data = dataFor(accountId))
                data->participantLeft[documentId] += 1;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::CollaborativeDocumentRenamed>(
        [=, this](const std::string& accountId,
                  const std::string& /*convId*/,
                  const std::string& documentId,
                  const std::string& name) {
            std::lock_guard<std::mutex> lock(mtx);
            if (auto* data = dataFor(accountId))
                data->renamedTo[documentId] = name;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::CollaborativeDocumentRemoved>(
        [=, this](const std::string& accountId,
                  const std::string& /*convId*/,
                  const std::string& documentId,
                  bool everywhere) {
            std::lock_guard<std::mutex> lock(mtx);
            if (auto* data = dataFor(accountId)) {
                if (everywhere)
                    data->removedEverywhere[documentId] += 1;
                else
                    data->removedLocally[documentId] += 1;
            }
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConfigurationSignal::CollaborativeAttachmentAdded>(
        [=, this](const std::string& accountId,
                  const std::string& /*convId*/,
                  const std::string& documentId,
                  const std::string& attachmentId) {
            std::lock_guard<std::mutex> lock(mtx);
            if (auto* data = dataFor(accountId))
                data->attachmentsAdded[documentId].emplace_back(attachmentId);
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);
}

std::string
CollabTest::createConversationWithBob()
{
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    auto convId = libjami::startConversation(aliceId);
    libjami::addConversationMember(aliceId, convId, bobUri);
    {
        std::unique_lock<std::mutex> lk(mtx);
        CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.requestReceived; }));
    }

    libjami::acceptConversationRequest(bobId, convId);
    {
        std::unique_lock<std::mutex> lk(mtx);
        CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return !bobData.conversationId.empty(); }));
        // Wait until alice sees bob's join, so both ends are settled.
        CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceData.messages.size() >= 2; }));
    }
    return convId;
}

std::filesystem::path
CollabTest::docRepoPath(const std::string& accountId, const std::string& docId)
{
    return fileutils::get_data_dir() / accountId / "conversations" / docId;
}

std::map<std::string, std::string>
CollabTest::documentEntry(const std::string& accountId, const std::string& convId, const std::string& docId)
{
    for (const auto& doc : libjami::getCollaborativeDocuments(accountId, convId))
        if (doc.at("id") == docId)
            return doc;
    return {};
}

bool
CollabTest::poll(std::function<bool()> pred, std::chrono::seconds timeout)
{
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (pred())
            return true;
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait_for(lk, 1s);
    }
    return pred();
}

bool
CollabTest::waitForCheckpoint(const std::string& accountId,
                              const std::string& convId,
                              const std::string& docId,
                              size_t atLeast)
{
    // A checkpoint lands after CHECKPOINT_IDLE (10 s) of inactivity.
    return poll([&] { return libjami::getCollaborativeDocumentHistory(accountId, convId, docId, 0).size() >= atLeast; },
                30s);
}

void
CollabTest::testCreateDocument()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceUri = aliceAccount->getUsername();
    auto convId = createConversationWithBob();

    auto docId = libjami::createCollaborativeDocument(aliceId, convId, "Notes", "text/plain");
    CPPUNIT_ASSERT(!docId.empty());

    // The creator holds the document: a standard swarm repository, with the
    // creator as admin.
    auto repoPath = docRepoPath(aliceId, docId);
    CPPUNIT_ASSERT(poll([&] { return std::filesystem::is_directory(repoPath); }));
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(repoPath / "admins" / (aliceUri + ".crt")));

    // Alice lists it, stored locally.
    auto entry = documentEntry(aliceId, convId, docId);
    CPPUNIT_ASSERT(!entry.empty());
    CPPUNIT_ASSERT_EQUAL("Notes"s, entry.at("displayName"));
    CPPUNIT_ASSERT_EQUAL("text/plain"s, entry.at("mimeType"));
    CPPUNIT_ASSERT_EQUAL(aliceUri, entry.at("author"));
    CPPUNIT_ASSERT_EQUAL("true"s, entry.at("storedLocally"));

    // The announcement reaches bob, who lists it too.
    CPPUNIT_ASSERT(poll([&] { return !documentEntry(bobId, convId, docId).empty(); }));
}

void
CollabTest::testNoAutoCloneOnAnnouncement()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto convId = createConversationWithBob();
    auto docId = libjami::createCollaborativeDocument(aliceId, convId, "Notes", "text/plain");
    CPPUNIT_ASSERT(!docId.empty());

    // Bob learns about the document...
    CPPUNIT_ASSERT(poll([&] { return !documentEntry(bobId, convId, docId).empty(); }));
    // ...but announcement is not replication: nothing is cloned until he opens.
    std::this_thread::sleep_for(5s);
    CPPUNIT_ASSERT(!std::filesystem::exists(docRepoPath(bobId, docId)));
    CPPUNIT_ASSERT_EQUAL("false"s, documentEntry(bobId, convId, docId).at("storedLocally"));
}

void
CollabTest::testOpenClonesAndJoins()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto convId = createConversationWithBob();

    // Alice creates and edits; her content lands in a checkpoint.
    auto docId = libjami::createCollaborativeDocument(aliceId, convId, "Notes", "text/plain");
    ClientReplica aliceReplica;
    libjami::applyCollaborativeUpdate(aliceId, convId, docId, aliceReplica.insert(0, "hello"));
    CPPUNIT_ASSERT(waitForCheckpoint(aliceId, convId, docId));

    // Bob opens: this clones the repository and writes his join.
    CPPUNIT_ASSERT(poll([&] { return !documentEntry(bobId, convId, docId).empty(); }));
    ClientReplica bobReplica;
    bobReplica.apply(libjami::openCollaborativeDocument(bobId, convId, docId));

    auto bobRepo = docRepoPath(bobId, docId);
    CPPUNIT_ASSERT(poll([&] { return std::filesystem::is_directory(bobRepo); }));
    // The clone carries bob's invitation (written by the serving holder), and
    // bob's join makes him a member of the document's own membership.
    CPPUNIT_ASSERT(poll([&] { return std::filesystem::is_regular_file(bobRepo / "members" / (bobUri + ".crt")); }));
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(bobRepo / "admins" / (aliceUri + ".crt")));
    CPPUNIT_ASSERT_EQUAL("true"s, documentEntry(bobId, convId, docId).at("storedLocally"));

    // And the checkpointed content converges.
    CPPUNIT_ASSERT(poll([&] {
        bobReplica.apply(libjami::collaborativeDocumentState(bobId, convId, docId));
        return bobReplica.text() == "hello";
    }));

    // Bob's join propagates back to alice's replica of the document.
    auto aliceRepo = docRepoPath(aliceId, docId);
    CPPUNIT_ASSERT(poll([&] { return std::filesystem::is_regular_file(aliceRepo / "members" / (bobUri + ".crt")); }));
}

void
CollabTest::testClosedHolderConvergesViaCheckpoints()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto convId = createConversationWithBob();
    auto docId = libjami::createCollaborativeDocument(aliceId, convId, "Notes", "text/plain");
    ClientReplica aliceReplica;

    // Bob opens (becomes a holder), then closes.
    CPPUNIT_ASSERT(poll([&] { return !documentEntry(bobId, convId, docId).empty(); }));
    libjami::openCollaborativeDocument(bobId, convId, docId);
    CPPUNIT_ASSERT(poll([&] { return std::filesystem::is_directory(docRepoPath(bobId, docId)); }));
    libjami::closeCollaborativeDocument(bobId, convId, docId);
    {
        std::lock_guard<std::mutex> lock(mtx);
        bobData.docUpdates.clear();
    }

    // Alice edits; the checkpoint replicates to bob although his document is
    // closed — holding is what subscribes a device, not having it open.
    libjami::applyCollaborativeUpdate(aliceId, convId, docId, aliceReplica.insert(0, "offline text"));
    CPPUNIT_ASSERT(waitForCheckpoint(aliceId, convId, docId));
    CPPUNIT_ASSERT(waitForCheckpoint(bobId, convId, docId));

    // Bob's client is told the document changed — an empty update, so it can
    // badge the document — but none of the content reaches a closed client.
    CPPUNIT_ASSERT(poll([&] {
        std::lock_guard<std::mutex> lock(mtx);
        return !bobData.docUpdates[docId].empty();
    }));
    {
        std::lock_guard<std::mutex> lock(mtx);
        for (const auto& update : bobData.docUpdates[docId])
            CPPUNIT_ASSERT(update.empty());
    }

    // Reopening hands the converged state over.
    ClientReplica bobReplica;
    bobReplica.apply(libjami::openCollaborativeDocument(bobId, convId, docId));
    CPPUNIT_ASSERT(poll([&] {
        bobReplica.apply(libjami::collaborativeDocumentState(bobId, convId, docId));
        return bobReplica.text() == "offline text";
    }));
}

void
CollabTest::testPerDeviceOptIn()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto convId = createConversationWithBob();

    auto docId = libjami::createCollaborativeDocument(aliceId, convId, "Notes", "text/plain");
    CPPUNIT_ASSERT(poll([&] { return !documentEntry(bobId, convId, docId).empty(); }));

    // Bob opens the document on his first device.
    libjami::openCollaborativeDocument(bobId, convId, docId);
    CPPUNIT_ASSERT(poll([&] { return std::filesystem::is_directory(docRepoPath(bobId, docId)); }));

    // Bob adds a second device.
    auto bobArchive = std::filesystem::current_path().string() + "/bob.gz";
    std::remove(bobArchive.c_str());
    bobAccount->exportArchive(bobArchive);
    std::map<std::string, std::string> details = libjami::getAccountTemplate("RING");
    details[ConfProperties::TYPE] = "RING";
    details[ConfProperties::DISPLAYNAME] = "BOB2";
    details[ConfProperties::ALIAS] = "BOB2";
    details[ConfProperties::UPNP_ENABLED] = "true";
    details[ConfProperties::ARCHIVE_PASSWORD] = "";
    details[ConfProperties::ARCHIVE_PATH] = bobArchive;
    auto newDeviceId = Manager::instance().addAccount(details);
    {
        // dataFor() reads bob2Id from daemon threads.
        std::lock_guard<std::mutex> lock(mtx);
        bob2Id = newDeviceId;
    }

    // The conversation syncs to the new device...
    {
        std::unique_lock<std::mutex> lk(mtx);
        CPPUNIT_ASSERT(cv.wait_for(lk, 60s, [&]() { return !bob2Data.conversationId.empty(); }));
    }
    // ...and it lists the document, but does not clone it: each device joins
    // by opening, the first device's opt-in is its own.
    CPPUNIT_ASSERT(poll([&] { return !documentEntry(bob2Id, convId, docId).empty(); }));
    std::this_thread::sleep_for(5s);
    CPPUNIT_ASSERT(!std::filesystem::exists(docRepoPath(bob2Id, docId)));
    CPPUNIT_ASSERT_EQUAL("false"s, documentEntry(bob2Id, convId, docId).at("storedLocally"));
}

void
CollabTest::testRenameAdminOnly()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto convId = createConversationWithBob();
    auto docId = libjami::createCollaborativeDocument(aliceId, convId, "Notes", "text/plain");

    // Bob opens, so he holds a replica the rename must reach.
    CPPUNIT_ASSERT(poll([&] { return !documentEntry(bobId, convId, docId).empty(); }));
    libjami::openCollaborativeDocument(bobId, convId, docId);
    CPPUNIT_ASSERT(poll([&] { return std::filesystem::is_directory(docRepoPath(bobId, docId)); }));

    // The creator (admin) renames: it propagates.
    libjami::setCollaborativeDocumentName(aliceId, convId, docId, "Renamed");
    CPPUNIT_ASSERT(poll([&] {
        std::lock_guard<std::mutex> lock(mtx);
        return bobData.renamedTo[docId] == "Renamed";
    }));
    CPPUNIT_ASSERT_EQUAL("Renamed"s, libjami::collaborativeDocumentName(bobId, convId, docId));

    // A plain member cannot: alice never sees bob's attempt.
    libjami::setCollaborativeDocumentName(bobId, convId, docId, "Vandalized");
    std::this_thread::sleep_for(10s);
    CPPUNIT_ASSERT_EQUAL("Renamed"s, libjami::collaborativeDocumentName(aliceId, convId, docId));
}

void
CollabTest::testAttachmentReplication()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto convId = createConversationWithBob();
    auto docId = libjami::createCollaborativeDocument(aliceId, convId, "Notes", "text/plain");

    CPPUNIT_ASSERT(poll([&] { return !documentEntry(bobId, convId, docId).empty(); }));
    libjami::openCollaborativeDocument(bobId, convId, docId);
    CPPUNIT_ASSERT(poll([&] { return std::filesystem::is_directory(docRepoPath(bobId, docId)); }));

    std::vector<uint8_t> payload {0x89, 0x50, 0x4E, 0x47, 0x00, 0x01, 0x02, 0x03};
    auto attachmentId = libjami::addCollaborativeAttachment(aliceId, convId, docId, payload);
    CPPUNIT_ASSERT(!attachmentId.empty());

    // Readable back where it was stored...
    CPPUNIT_ASSERT(libjami::collaborativeAttachment(aliceId, convId, docId, attachmentId) == payload);

    // ...and it replicates to the other holder.
    CPPUNIT_ASSERT(
        poll([&] { return libjami::collaborativeAttachment(bobId, convId, docId, attachmentId) == payload; }));
}

void
CollabTest::testRemoveDocumentEverywhere()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto convId = createConversationWithBob();
    auto docId = libjami::createCollaborativeDocument(aliceId, convId, "Notes", "text/plain");

    CPPUNIT_ASSERT(poll([&] { return !documentEntry(bobId, convId, docId).empty(); }));
    libjami::openCollaborativeDocument(bobId, convId, docId);
    CPPUNIT_ASSERT(poll([&] { return std::filesystem::is_directory(docRepoPath(bobId, docId)); }));

    // Only the author may retire a document; the announcement edit does it.
    CPPUNIT_ASSERT(libjami::removeCollaborativeDocument(aliceId, convId, docId));

    CPPUNIT_ASSERT(poll([&] {
        std::lock_guard<std::mutex> lock(mtx);
        return bobData.removedEverywhere[docId] > 0;
    }));
    CPPUNIT_ASSERT(poll([&] { return !std::filesystem::exists(docRepoPath(bobId, docId)); }));
    CPPUNIT_ASSERT(poll([&] { return !std::filesystem::exists(docRepoPath(aliceId, docId)); }));
    CPPUNIT_ASSERT(documentEntry(aliceId, convId, docId).empty());
    CPPUNIT_ASSERT(documentEntry(bobId, convId, docId).empty());
}

void
CollabTest::testRemoveDocumentLocallyAndReopen()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto convId = createConversationWithBob();
    auto docId = libjami::createCollaborativeDocument(aliceId, convId, "Notes", "text/plain");
    ClientReplica aliceReplica;
    libjami::applyCollaborativeUpdate(aliceId, convId, docId, aliceReplica.insert(0, "kept"));
    CPPUNIT_ASSERT(waitForCheckpoint(aliceId, convId, docId));

    CPPUNIT_ASSERT(poll([&] { return !documentEntry(bobId, convId, docId).empty(); }));
    libjami::openCollaborativeDocument(bobId, convId, docId);
    CPPUNIT_ASSERT(poll([&] { return std::filesystem::is_directory(docRepoPath(bobId, docId)); }));
    libjami::closeCollaborativeDocument(bobId, convId, docId);

    // Local removal reclaims the disk, keeps the listing.
    CPPUNIT_ASSERT(libjami::removeCollaborativeDocumentLocally(bobId, convId, docId));
    CPPUNIT_ASSERT(poll([&] { return !std::filesystem::exists(docRepoPath(bobId, docId)); }));
    auto entry = documentEntry(bobId, convId, docId);
    CPPUNIT_ASSERT(!entry.empty());
    CPPUNIT_ASSERT_EQUAL("false"s, entry.at("storedLocally"));

    // Nothing replicates it back while removed.
    libjami::applyCollaborativeUpdate(aliceId, convId, docId, aliceReplica.insert(4, " away"));
    CPPUNIT_ASSERT(waitForCheckpoint(aliceId, convId, docId, 2));
    std::this_thread::sleep_for(5s);
    CPPUNIT_ASSERT(!std::filesystem::exists(docRepoPath(bobId, docId)));

    // Reopening brings it back — bob is still a member, no new invitation is
    // needed — and it converges on everything missed meanwhile.
    ClientReplica bobReplica;
    bobReplica.apply(libjami::openCollaborativeDocument(bobId, convId, docId));
    CPPUNIT_ASSERT(poll([&] { return std::filesystem::is_directory(docRepoPath(bobId, docId)); }));
    CPPUNIT_ASSERT(poll([&] {
        bobReplica.apply(libjami::collaborativeDocumentState(bobId, convId, docId));
        return bobReplica.text() == "kept away";
    }));
}

void
CollabTest::testDocumentBanRefusesClone()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto convId = createConversationWithBob();
    auto docId = libjami::createCollaborativeDocument(aliceId, convId, "Notes", "text/plain");

    // Bob joins the document, then drops his local copy.
    CPPUNIT_ASSERT(poll([&] { return !documentEntry(bobId, convId, docId).empty(); }));
    libjami::openCollaborativeDocument(bobId, convId, docId);
    CPPUNIT_ASSERT(poll([&] { return std::filesystem::is_directory(docRepoPath(bobId, docId)); }));
    // Wait for bob's membership to reach alice, who must know him to ban him.
    auto aliceRepo = docRepoPath(aliceId, docId);
    CPPUNIT_ASSERT(poll([&] { return std::filesystem::is_regular_file(aliceRepo / "members" / (bobUri + ".crt")); }));
    libjami::closeCollaborativeDocument(bobId, convId, docId);
    CPPUNIT_ASSERT(libjami::removeCollaborativeDocumentLocally(bobId, convId, docId));
    CPPUNIT_ASSERT(poll([&] { return !std::filesystem::exists(docRepoPath(bobId, docId)); }));

    // The document admin bans bob from the document — the document has its own
    // membership, addressed by its own id.
    libjami::removeConversationMember(aliceId, docId, bobUri);
    CPPUNIT_ASSERT(
        poll([&] { return std::filesystem::is_regular_file(aliceRepo / "banned" / "members" / (bobUri + ".crt")); }));

    // Bob remains a member of the parent conversation, but reopening is
    // refused: document bans take precedence.
    libjami::openCollaborativeDocument(bobId, convId, docId);
    std::this_thread::sleep_for(10s);
    CPPUNIT_ASSERT(!std::filesystem::exists(docRepoPath(bobId, docId)));
}

void
CollabTest::testParentConversationLeaveLeavesDocuments()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto convId = createConversationWithBob();
    auto docId = libjami::createCollaborativeDocument(aliceId, convId, "Notes", "text/plain");

    CPPUNIT_ASSERT(poll([&] { return !documentEntry(bobId, convId, docId).empty(); }));
    libjami::openCollaborativeDocument(bobId, convId, docId);
    auto aliceRepo = docRepoPath(aliceId, docId);
    CPPUNIT_ASSERT(poll([&] { return std::filesystem::is_regular_file(aliceRepo / "members" / (bobUri + ".crt")); }));
    libjami::closeCollaborativeDocument(bobId, convId, docId);

    // Bob leaves the conversation. His device leaves each held document the
    // way a conversation is left: a leave commit, fetched by a member, then
    // the local repository goes.
    libjami::removeConversation(bobId, convId);
    {
        std::unique_lock<std::mutex> lk(mtx);
        CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobData.conversationRemoved; }));
    }
    CPPUNIT_ASSERT(poll([&] { return !std::filesystem::exists(docRepoPath(bobId, docId)); }));

    // Alice's replica of the document saw the leave: bob is no longer a member.
    CPPUNIT_ASSERT(poll([&] { return !std::filesystem::exists(aliceRepo / "members" / (bobUri + ".crt")); }));
}

void
CollabTest::testRealtimeUpdatePropagation()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto convId = createConversationWithBob();
    auto docId = libjami::createCollaborativeDocument(aliceId, convId, "Notes", "text/plain");

    ClientReplica aliceReplica;
    aliceReplica.apply(libjami::openCollaborativeDocument(aliceId, convId, docId));
    CPPUNIT_ASSERT(poll([&] { return !documentEntry(bobId, convId, docId).empty(); }));
    ClientReplica bobReplica;
    bobReplica.apply(libjami::openCollaborativeDocument(bobId, convId, docId));
    CPPUNIT_ASSERT(poll([&] { return std::filesystem::is_directory(docRepoPath(bobId, docId)); }));

    // Give the realtime channels a moment to establish, then edit.
    std::this_thread::sleep_for(2s);
    {
        std::lock_guard<std::mutex> lock(mtx);
        bobData.docUpdates.clear();
    }
    libjami::applyCollaborativeUpdate(aliceId, convId, docId, aliceReplica.insert(0, "live"));

    // The update reaches bob's client well before any checkpoint could
    // (checkpoints wait out 10 s of idle time).
    CPPUNIT_ASSERT(poll(
        [&] {
            std::lock_guard<std::mutex> lock(mtx);
            return !bobData.docUpdates[docId].empty();
        },
        5s));
    {
        std::lock_guard<std::mutex> lock(mtx);
        for (const auto& update : bobData.docUpdates[docId])
            bobReplica.apply(update);
    }
    CPPUNIT_ASSERT_EQUAL("live"s, bobReplica.text());
}

void
CollabTest::testAwareness()
{
    std::cout << "\nRunning test: " << __func__ << std::endl;
    connectSignals();

    auto convId = createConversationWithBob();
    auto docId = libjami::createCollaborativeDocument(aliceId, convId, "Notes", "text/plain");

    libjami::openCollaborativeDocument(aliceId, convId, docId);
    CPPUNIT_ASSERT(poll([&] { return !documentEntry(bobId, convId, docId).empty(); }));
    libjami::openCollaborativeDocument(bobId, convId, docId);
    CPPUNIT_ASSERT(poll([&] { return std::filesystem::is_directory(docRepoPath(bobId, docId)); }));
    std::this_thread::sleep_for(2s);

    // Alice shares her presence; bob's client hears about it.
    auto state = "{\"cursor\":42}"s;
    libjami::setCollaborativeAwareness(aliceId, convId, docId, state);
    CPPUNIT_ASSERT(poll([&] {
        std::lock_guard<std::mutex> lock(mtx);
        const auto& states = bobData.awareness[docId];
        return !states.empty() && states.back() == state;
    }));

    // Closing withdraws it.
    libjami::closeCollaborativeDocument(aliceId, convId, docId);
    CPPUNIT_ASSERT(poll([&] {
        std::lock_guard<std::mutex> lock(mtx);
        return bobData.participantLeft[docId] > 0;
    }));
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::CollabTest::name())
