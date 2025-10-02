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

#include "manager.h"
#include "jamidht/conversationrepository.h"
#include "jamidht/gitserver.h"
#include "jamidht/jamiaccount.h"
#include "../../test_runner.h"
#include "jami.h"
#include "base64.h"
#include "fileutils.h"
#include "account_const.h"
#include "common.h"

#include <git2.h>

#include <dhtnet/connectionmanager.h>

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <condition_variable>
#include <string>
#include <fstream>
#include <streambuf>
#include <filesystem>

using namespace std::string_literals;
using namespace libjami::Account;

namespace jami {
namespace test {

class ConversationRepositoryTest : public CppUnit::TestFixture
{
public:
    ConversationRepositoryTest()
    {
        // Init daemon
        libjami::init(
            libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    }
    ~ConversationRepositoryTest() { libjami::fini(); }
    static std::string name() { return "ConversationRepository"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    std::string bobId;

private:
    void testCreateRepository();
    void testAddSomeMessages();
    void testLogMessages();
    void testMerge();
    void testFFMerge();
    void testDiff();
    void testLogClosestCommitReachable();
    void testMergeProfileWithConflict();
<<<<<<< PATCH SET (ce54cd ConversationRepositoryTest: add tests for uri and join verif)
    void testValidSignatureForCommit();
    void testInvalidSignatureForCommit();
    void testMalformedCommit();
    void testMalformedCommitUri();
    void testInvalidJoin();
    std::string addCommit(git_repository* repo,
                          const std::shared_ptr<JamiAccount> account,
                          const std::string& branch,
                          const std::string& commit_msg);
=======

>>>>>>> BASE      (161cd0 Revwalk fixes - in progress)
    void addAll(git_repository* repo);
    bool merge_in_main(const std::shared_ptr<JamiAccount> account,
                       git_repository* repo,
                       const std::string& commit_ref);

    CPPUNIT_TEST_SUITE(ConversationRepositoryTest);
<<<<<<< PATCH SET (ce54cd ConversationRepositoryTest: add tests for uri and join verif)
    CPPUNIT_TEST(testCreateRepository);          // Passes
    CPPUNIT_TEST(testAddSomeMessages);           // Passes
    CPPUNIT_TEST(testLogMessages);               // Passes
    CPPUNIT_TEST(testMerge);                     // Passes
    CPPUNIT_TEST(testFFMerge);                   // Passes
    CPPUNIT_TEST(testDiff);                      // Passes
    CPPUNIT_TEST(testMergeProfileWithConflict);  // Passes
    CPPUNIT_TEST(testValidSignatureForCommit);   // Passes
    CPPUNIT_TEST(testInvalidSignatureForCommit); // Passes
    CPPUNIT_TEST(testMalformedCommit);           // Passes
    CPPUNIT_TEST(testMalformedCommitUri);        // Passes
    CPPUNIT_TEST(testInvalidJoin);               // Passes
=======
    CPPUNIT_TEST(testCreateRepository);
    CPPUNIT_TEST(testAddSomeMessages);
    CPPUNIT_TEST(testLogMessages);
    CPPUNIT_TEST(testMerge);
    CPPUNIT_TEST(testFFMerge);
    CPPUNIT_TEST(testDiff);
    CPPUNIT_TEST(testMergeProfileWithConflict);
    CPPUNIT_TEST(testLogClosestCommitReachable);

>>>>>>> BASE      (161cd0 Revwalk fixes - in progress)
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ConversationRepositoryTest,
                                      ConversationRepositoryTest::name());

void
ConversationRepositoryTest::setUp()
{
    auto actors = load_actors_and_wait_for_announcement("actors/alice-bob.yml");
    aliceId = actors["alice"];
    bobId = actors["bob"];
}

void
ConversationRepositoryTest::tearDown()
{
    wait_for_removal_of({aliceId, bobId});
}

void
ConversationRepositoryTest::testCreateRepository()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceDeviceId = DeviceId(std::string(aliceAccount->currentDeviceId()));
    auto uri = aliceAccount->getUsername();

    auto repository = ConversationRepository::createConversation(aliceAccount);

    // Assert that repository exists
    CPPUNIT_ASSERT(repository != nullptr);
    auto repoPath = fileutils::get_data_dir() / aliceAccount->getAccountID()
                    / "conversations" / repository->id();
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));

    // Assert that first commit is signed by alice
    git_repository* repo;
    CPPUNIT_ASSERT(git_repository_open(&repo, repoPath.c_str()) == 0);

    // 1. Verify that last commit is correctly signed by alice
    git_oid commit_id;
    CPPUNIT_ASSERT(git_reference_name_to_id(&commit_id, repo, "HEAD") == 0);

    git_buf signature = {}, signed_data = {};
    git_commit_extract_signature(&signature, &signed_data, repo, &commit_id, "signature");
    auto pk = base64::decode(std::string(signature.ptr, signature.ptr + signature.size));
    auto data = std::vector<uint8_t>(signed_data.ptr, signed_data.ptr + signed_data.size);
    git_repository_free(repo);

    CPPUNIT_ASSERT(aliceAccount->identity().second->getPublicKey().checkSignature(data, pk));

    // 2. Check created files
    auto CRLsPath = repoPath / "CRLs" / aliceDeviceId.toString();
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));

    auto adminCrt = repoPath / "admins" / (uri + ".crt");
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(adminCrt));

    auto crt = std::ifstream(adminCrt);
    std::string adminCrtStr((std::istreambuf_iterator<char>(crt)), std::istreambuf_iterator<char>());

    auto cert = aliceAccount->identity().second;
    auto deviceCert = cert->toString(false);
    auto parentCert = cert->issuer->toString(true);

    CPPUNIT_ASSERT(adminCrtStr == parentCert);

    auto deviceCrt = repoPath / "devices" / (aliceDeviceId.toString() + ".crt");
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(deviceCrt));

    crt = std::ifstream(deviceCrt);
    std::string deviceCrtStr((std::istreambuf_iterator<char>(crt)),
                             std::istreambuf_iterator<char>());

    CPPUNIT_ASSERT(deviceCrtStr == deviceCert);
}

void
ConversationRepositoryTest::testAddSomeMessages()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto repository = ConversationRepository::createConversation(aliceAccount);

    auto id1 = repository->commitMessage("Commit 1");
    auto id2 = repository->commitMessage("Commit 2");
    auto id3 = repository->commitMessage("Commit 3");

    auto messages = repository->log();
    CPPUNIT_ASSERT(messages.size() == 4 /* 3 + initial */);
    CPPUNIT_ASSERT(messages[0].id == id3);
    CPPUNIT_ASSERT(messages[0].parents.front() == id2);
    CPPUNIT_ASSERT(messages[0].commit_msg == "Commit 3");
    CPPUNIT_ASSERT(messages[0].author.name == messages[3].author.name);
    CPPUNIT_ASSERT(messages[0].author.email == messages[3].author.email);
    CPPUNIT_ASSERT(messages[1].id == id2);
    CPPUNIT_ASSERT(messages[1].parents.front() == id1);
    CPPUNIT_ASSERT(messages[1].commit_msg == "Commit 2");
    CPPUNIT_ASSERT(messages[1].author.name == messages[3].author.name);
    CPPUNIT_ASSERT(messages[1].author.email == messages[3].author.email);
    CPPUNIT_ASSERT(messages[2].id == id1);
    CPPUNIT_ASSERT(messages[2].commit_msg == "Commit 1");
    CPPUNIT_ASSERT(messages[2].author.name == messages[3].author.name);
    CPPUNIT_ASSERT(messages[2].author.email == messages[3].author.email);
    CPPUNIT_ASSERT(messages[2].parents.front() == repository->id());
    // Check sig
    CPPUNIT_ASSERT(
        aliceAccount->identity().second->getPublicKey().checkSignature(messages[0].signed_content,
                                                                       messages[0].signature));
    CPPUNIT_ASSERT(
        aliceAccount->identity().second->getPublicKey().checkSignature(messages[1].signed_content,
                                                                       messages[1].signature));
    CPPUNIT_ASSERT(
        aliceAccount->identity().second->getPublicKey().checkSignature(messages[2].signed_content,
                                                                       messages[2].signature));
}

void
ConversationRepositoryTest::testLogMessages()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto repository = ConversationRepository::createConversation(aliceAccount);

    auto id1 = repository->commitMessage("Commit 1");
    auto id2 = repository->commitMessage("Commit 2");
    auto id3 = repository->commitMessage("Commit 3");

    LogOptions options;
    options.from = repository->id();
    options.nbOfCommits = 1;
    auto messages = repository->log(options);
    CPPUNIT_ASSERT(messages.size() == 1);
    CPPUNIT_ASSERT(messages[0].id == repository->id());
    options.from = id2;
    options.nbOfCommits = 2;
    messages = repository->log(options);
    CPPUNIT_ASSERT(messages.size() == 2);
    CPPUNIT_ASSERT(messages[0].id == id2);
    CPPUNIT_ASSERT(messages[1].id == id1);
    options.from = repository->id();
    options.nbOfCommits = 3;
    messages = repository->log(options);
    CPPUNIT_ASSERT(messages.size() == 1);
    CPPUNIT_ASSERT(messages[0].id == repository->id());
}

void
ConversationRepositoryTest::addAll(git_repository* repo)
{
    // git add -A
    git_index* index_ptr = nullptr;
    if (git_repository_index(&index_ptr, repo) < 0)
        return;
    GitIndex index {index_ptr, git_index_free};
    git_strarray array = {nullptr, 0};
    git_index_add_all(index.get(), &array, 0, nullptr, nullptr);
    git_index_write(index.get());
    git_strarray_free(&array);
}

void
ConversationRepositoryTest::testMerge()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto repository = ConversationRepository::createConversation(aliceAccount);

    // Assert that repository exists
    CPPUNIT_ASSERT(repository != nullptr);
    auto repoPath = fileutils::get_data_dir() / aliceAccount->getAccountID()
                    / "conversations" / repository->id();
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));

    // Assert that first commit is signed by alice
    git_repository* repo;
    CPPUNIT_ASSERT(git_repository_open(&repo, repoPath.c_str()) == 0);
    auto id1 = addCommit(repo, aliceAccount, "main", "Commit 1");

    git_reference* ref = nullptr;
    git_commit* commit = nullptr;
    git_oid commit_id;
    git_oid_fromstr(&commit_id, repository->id().c_str());
    git_commit_lookup(&commit, repo, &commit_id);
    git_branch_create(&ref, repo, "to_merge", commit, false);
    git_reference_free(ref);
    git_repository_set_head(repo, "refs/heads/to_merge");

    auto id2 = addCommit(repo, aliceAccount, "to_merge", "Commit 2");
    git_repository_free(repo);

    // This will create a merge commit
    repository->merge(id2);

    CPPUNIT_ASSERT(repository->log().size() == 4 /* Initial, commit 1, 2, merge */);
}

void
ConversationRepositoryTest::testFFMerge()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto repository = ConversationRepository::createConversation(aliceAccount);

    // Assert that repository exists
    CPPUNIT_ASSERT(repository != nullptr);
    auto repoPath = fileutils::get_data_dir() / aliceAccount->getAccountID()
                    / "conversations" / repository->id();
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));

    // Assert that first commit is signed by alice
    git_repository* repo;
    CPPUNIT_ASSERT(git_repository_open(&repo, repoPath.c_str()) == 0);
    auto id1 = addCommit(repo, aliceAccount, "main", "Commit 1");

    git_reference* ref = nullptr;
    git_commit* commit = nullptr;
    git_oid commit_id;
    git_oid_fromstr(&commit_id, id1.c_str());
    git_commit_lookup(&commit, repo, &commit_id);
    git_branch_create(&ref, repo, "to_merge", commit, false);
    git_reference_free(ref);
    git_repository_set_head(repo, "refs/heads/to_merge");

    auto id2 = addCommit(repo, aliceAccount, "to_merge", "Commit 2");
    git_repository_free(repo);

    // This will use a fast forward merge
    repository->merge(id2);

    CPPUNIT_ASSERT(repository->log().size() == 3 /* Initial, commit 1, 2 */);
}

void
ConversationRepositoryTest::testDiff()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceDeviceId = DeviceId(std::string(aliceAccount->currentDeviceId()));
    auto uri = aliceAccount->getUsername();
    auto repository = ConversationRepository::createConversation(aliceAccount);

    auto id1 = repository->commitMessage("Commit 1");
    auto id2 = repository->commitMessage("Commit 2");
    auto id3 = repository->commitMessage("Commit 3");

    auto diff = repository->diffStats(id2, id1);
    CPPUNIT_ASSERT(ConversationRepository::changedFiles(diff).empty());
    diff = repository->diffStats(id1);
    auto changedFiles = ConversationRepository::changedFiles(diff);
    CPPUNIT_ASSERT(!changedFiles.empty());
    CPPUNIT_ASSERT(changedFiles[0] == "admins/" + uri + ".crt");
    CPPUNIT_ASSERT(changedFiles[1] == "devices/" + aliceDeviceId.toString() + ".crt");
}

void
ConversationRepositoryTest::testMergeProfileWithConflict()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto repository = ConversationRepository::createConversation(aliceAccount);

    // Assert that repository exists
    CPPUNIT_ASSERT(repository != nullptr);
    auto repoPath = fileutils::get_data_dir() / aliceAccount->getAccountID()
                    / "conversations" / repository->id();
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));

    // Assert that first commit is signed by alice
    git_repository* repo;
    CPPUNIT_ASSERT(git_repository_open(&repo, repoPath.c_str()) == 0);

    auto profile = std::ofstream(repoPath / "profile.vcf");
    if (profile.is_open()) {
        profile << "TITLE: SWARM\n";
        profile << "SUBTITLE: Some description\n";
        profile << "AVATAR: BASE64\n";
        profile.close();
    }
    addAll(repo);
    auto id1 = addCommit(repo, aliceAccount, "main", "add profile");
    profile = std::ofstream(repoPath / "profile.vcf");
    if (profile.is_open()) {
        profile << "TITLE: SWARM\n";
        profile << "SUBTITLE: New description\n";
        profile << "AVATAR: BASE64\n";
        profile.close();
    }
    addAll(repo);
    auto id2 = addCommit(repo, aliceAccount, "main", "modify profile");

    git_reference* ref = nullptr;
    git_commit* commit = nullptr;
    git_oid commit_id;
    git_oid_fromstr(&commit_id, id1.c_str());
    git_commit_lookup(&commit, repo, &commit_id);
    git_branch_create(&ref, repo, "to_merge", commit, false);
    git_reference_free(ref);
    git_repository_set_head(repo, "refs/heads/to_merge");

    profile = std::ofstream(repoPath / "profile.vcf");
    if (profile.is_open()) {
        profile << "TITLE: SWARM\n";
        profile << "SUBTITLE: Another description\n";
        profile << "AVATAR: BASE64\n";
        profile.close();
    }
    addAll(repo);
    auto id3 = addCommit(repo, aliceAccount, "to_merge", "modify profile merge");

    // This will create a merge commit
    repository->merge(id3);
    CPPUNIT_ASSERT(repository->log().size() == 5 /* Initial, add, modify 1, modify 2, merge */);
}

void
ConversationRepositoryTest::testLogClosestCommitReachable()
{
    // Setup account and repository
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto repository = ConversationRepository::createConversation(aliceAccount);
    CPPUNIT_ASSERT(repository != nullptr);

    // Determine repo path
    auto repoPath = fileutils::get_data_dir() / aliceAccount->getAccountID()
                    / "conversations" / repository->id();
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));
    git_repository* repo;
    CPPUNIT_ASSERT(git_repository_open(&repo, repoPath.c_str()) == 0);

    std::string displayName = aliceAccount->getDisplayName();
    std::string deviceId = std::string(aliceAccount->currentDeviceId()); // "email"

    // Add initial commits to repo
    std::string id_clear = addCommit(repo, aliceAccount, "main", "{\"body\":\"CLEAR\",\"type\":\"text/plain\"}");
    CPPUNIT_ASSERT(id_clear != "");
    std::string id_2 = addCommit(repo, aliceAccount, "main", "{\"body\":\"2\",\"type\":\"text/plain\"}");
    CPPUNIT_ASSERT(id_2 != "");
    std::string id_3 = addCommit(repo, aliceAccount, "main", "{\"body\":\"3\",\"type\":\"text/plain\"}");
    CPPUNIT_ASSERT(id_3 != "");

    // Create new branch beginning from id_clear
    git_reference* to_merge_ref;
    git_oid msgclear_oid;
    git_commit* msgclear_commit;
    git_oid_fromstr(&msgclear_oid, id_clear.c_str());
    git_commit_lookup(&msgclear_commit, repo, &msgclear_oid);
    int to_merge_creation = git_branch_create(&to_merge_ref, repo, "to_merge", msgclear_commit, false);
    CPPUNIT_ASSERT(to_merge_creation == 0);

    // Add missing message '1' to the "to_merge" branch
    git_repository_set_head(repo, "refs/heads/to_merge");
    std::string id_1 = addCommit(repo, aliceAccount, "to_merge", "{\"body\":\"1\",\"type\":\"text/plain\"}");
    CPPUNIT_ASSERT(id_1 != "");

    git_repository_set_head(repo, "refs/heads/to_merge");

    // Manually create merge of messages '1' and '3' for "main" and "to_merge" branches
    // Get parents
    git_oid msg1_oid, msg3_oid;
    git_oid_fromstr(&msg1_oid, id_1.c_str());
    git_oid_fromstr(&msg3_oid, id_3.c_str());
    git_commit* msg1_commit = nullptr;
    git_commit* msg3_commit = nullptr;
    git_commit_lookup(&msg1_commit, repo, &msg1_oid);
    git_commit_lookup(&msg3_commit, repo, &msg3_oid);

    // Merge onto "to_merge" branch
    git_oid msgmerge_oid1;
    git_signature* msgmerge_sig1 = nullptr;
    git_signature_now(&msgmerge_sig1, displayName.c_str(), deviceId.c_str());
    std::string msgmerge_commit_message1 = "Merge commit '" + id_1 + "'";
    git_tree* tree1 = nullptr;
    git_commit_tree(&tree1, msg1_commit);
    git_commit* const msgmerge_parents[2] = {msg1_commit, msg3_commit};
    int create_first_merge = git_commit_create(&msgmerge_oid1, repo, "refs/heads/to_merge", msgmerge_sig1, msgmerge_sig1, nullptr, msgmerge_commit_message1.c_str(), tree1, 2, msgmerge_parents);
    CPPUNIT_ASSERT(create_first_merge == 0);

    // Merge onto "main" branch
    git_oid msgmerge_oid2;
    git_signature* msgmerge_sig2 = nullptr;
    git_signature_now(&msgmerge_sig2, displayName.c_str(), deviceId.c_str());
    std::string msgmerge_commit_message2 = "Merge commit '" + id_3 + "'";
    git_tree* tree2 = nullptr;
    git_commit_tree(&tree2, msg3_commit);
    git_commit* const msgmerge_parents2[2] = {msg3_commit, msg1_commit};
    int create_second_merge = git_commit_create(&msgmerge_oid2, repo, "refs/heads/main", msgmerge_sig2, msgmerge_sig2, nullptr, msgmerge_commit_message2.c_str(), tree2, 2, msgmerge_parents2);
    CPPUNIT_ASSERT(create_second_merge == 0);

    // Add commit message '4' to the "to_merge" branch
    git_repository_set_head(repo, "refs/heads/to_merge");
    std::string id_4 = addCommit(repo, aliceAccount, "to_merge", "{\"body\":\"4\",\"type\":\"text/plain\"}");
    CPPUNIT_ASSERT(id_4 != "");

    // Merge "to_merge" branch onto "main" branch
    std::pair<bool, std::string> to_merge_onto_main = repository->merge(id_4);
    CPPUNIT_ASSERT(to_merge_onto_main.first);

    // Continue with commit message '5' on "main" branch
    git_repository_set_head(repo, "refs/heads/main");
    std::string id_5 = addCommit(repo, aliceAccount, "main", "{\"body\":\"5\",\"type\":\"text/plain\"}");
    CPPUNIT_ASSERT(id_5 != "");

    // Determine if proper iteration took place
    std::string from = id_5;
    std::string to = id_3;
    std::vector<jami::ConversationCommit> conversation_items = repository->log(LogOptions {from, to});
    CPPUNIT_ASSERT(conversation_items.size() == 6);

    // Free resources
    git_reference_free(to_merge_ref);
    git_commit_free(msgclear_commit);
    git_commit_free(msg1_commit);
    git_commit_free(msg3_commit);
    git_signature_free(msgmerge_sig1);
    git_signature_free(msgmerge_sig2);
    git_tree_free(tree1);
    git_tree_free(tree2);
    git_repository_free(repo);
}
<<<<<<< PATCH SET (ce54cd ConversationRepositoryTest: add tests for uri and join verif)

void
ConversationRepositoryTest::testInvalidSignatureForCommit()
{
    // Get Alice and Bob's accounts
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    // Create a repository associated with Alice's account
    auto repository = ConversationRepository::createConversation(aliceAccount);
    // Assert that the repository exists
    CPPUNIT_ASSERT(repository != nullptr);
    // Get the path to the conversation repository
    auto repoPath = fileutils::get_data_dir() / aliceAccount->getAccountID() / "conversations"
                    / repository->id();
    // Assert that the repository path is a directory
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));

    // Assert that the git repo was opened successfully
    git_repository* repo;
    CPPUNIT_ASSERT(git_repository_open(&repo, repoPath.c_str()) == 0);

    // Make a malicious commit that disguises itself as being sent from Alice
    // Retrieve current index
    git_index* index_ptr = nullptr;
    CPPUNIT_ASSERT(git_repository_index(&index_ptr, repo) == 0);
    GitIndex index {index_ptr, git_index_free};

    // Write the index to a new tree
    git_oid tree_id;
    // Assert that the tree was written successfully
    CPPUNIT_ASSERT(git_index_write_tree(&tree_id, index.get()) == 0);

    // Lookup the newly created tree
    git_tree* tree_ptr = nullptr;
    CPPUNIT_ASSERT(git_tree_lookup(&tree_ptr, repo, &tree_id) == 0);
    GitTree tree = {tree_ptr, git_tree_free};

    // Create a commit object
    git_oid commit_id;
    CPPUNIT_ASSERT(git_reference_name_to_id(&commit_id, repo, "HEAD") == 0);

    // Lookup the HEAD commit
    git_commit* head_ptr = nullptr;
    CPPUNIT_ASSERT(git_commit_lookup(&head_ptr, repo, &commit_id) == 0);
    GitCommit head_commit {head_ptr, git_commit_free};

    // The last argument of git_commit_create_buffer is of type
    // 'const git_commit **' in all versions of libgit2 except 1.8.0,
    // 1.8.1 and 1.8.3, in which it is of type 'git_commit *const *'.
#if LIBGIT2_VER_MAJOR == 1 && LIBGIT2_VER_MINOR == 8 \
    && (LIBGIT2_VER_REVISION == 0 || LIBGIT2_VER_REVISION == 1 || LIBGIT2_VER_REVISION == 3)
    git_commit* const head_ref[1] = {head_commit.get()};
#else
    const git_commit* head_ref[1] = {head_commit.get()};
#endif

    // Create the signature for the commit with the current time and using the
    // Alice's device ID (i.e. her public key for her device)
    git_signature* sig = nullptr;
    CPPUNIT_ASSERT(git_signature_now(&sig,
                                     aliceAccount->getDisplayName().c_str(),
                                     std::string(aliceAccount->currentDeviceId()).c_str())
                   == 0);

    // Create a signed commit object and place it in a buffer
    git_buf to_sign = {};
    CPPUNIT_ASSERT(
        git_commit_create_buffer(&to_sign,
                                 repo,
                                 sig,
                                 sig,
                                 nullptr,
                                 "{\"body\":\"malicious intent\",\"type\":\"text/plain\"}",
                                 tree.get(),
                                 1,
                                 &head_ref[0])
        == 0);
    git_signature_free(sig);

    // git commit -S
    auto to_sign_vec = std::vector<uint8_t>(to_sign.ptr, to_sign.ptr + to_sign.size);
    // Sign the commit using Bob's private key
    std::vector<unsigned char> signed_buf = bobAccount->identity().first->sign(to_sign_vec);
    std::string signed_str = base64::encode(signed_buf);
    CPPUNIT_ASSERT(git_commit_create_with_signature(&commit_id,
                                                    repo,
                                                    to_sign.ptr,
                                                    signed_str.c_str(),
                                                    "signature")
                   == 0);

    // Dispose the signature buffer
    git_buf_dispose(&to_sign);

    // Move commit to main branch
    git_reference* ref_ptr = nullptr;
    CPPUNIT_ASSERT(
        (git_reference_create(&ref_ptr, repo, "refs/heads/main", &commit_id, true, nullptr) == 0));

    CPPUNIT_ASSERT(repo);

    // Lookup the commit
    git_commit* commit_ptr = nullptr;
    CPPUNIT_ASSERT(git_commit_lookup(&commit_ptr, repo, &commit_id) == 0);

    // Get the author and user device, and assert that the user is not valid at the commit
    const git_signature* author = git_commit_author(commit_ptr);
    std::string userDevice = author->email;

    // Extract the signature and signature data of commit_id
    git_buf extracted_sig = {}, extracted_sig_data = {};
    // Extract the signature block and signature content from the commit
    git_commit_extract_signature(&extracted_sig, &extracted_sig_data, repo, &commit_id, "signature");
    CPPUNIT_ASSERT(!repository->isValidUserAtCommit(userDevice,
                                                    git_oid_tostr_s(&commit_id),
                                                    extracted_sig,
                                                    extracted_sig_data));

    // Free git objects
    git_reference_free(ref_ptr);
    git_repository_free(repo);
    git_buf_dispose(&extracted_sig);
    git_buf_dispose(&extracted_sig_data);
}

void
ConversationRepositoryTest::testMalformedCommit()
{
    // Register signals
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;

    // Variable to be set to true on detection of malformed commit
    bool isMalformed = false;

    // Signals for malformed commits in validCommits function
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::OnConversationError>(
            [&](const std::string& accountId,
                const std::string& conversationId,
                int code,
                auto malformedSpec) {
                if (code == EVALIDFETCH) {
                    isMalformed = true;
                }
                cv.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);

    // Get Alice's account
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    // Create a repository associated with Alice's account
    auto repository = ConversationRepository::createConversation(aliceAccount);
    // Assert that repository exists
    CPPUNIT_ASSERT(repository != nullptr);
    // Get the path to the conversation repository
    auto repoPath = fileutils::get_data_dir() / aliceAccount->getAccountID() / "conversations"
                    / repository->id();
    // Assert that the repository path is a directory
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));

    // Assert that the git repo was opened successfully
    git_repository* repo;
    CPPUNIT_ASSERT(git_repository_open(&repo, repoPath.c_str()) == 0);

    // Create a malformed join commit
    auto malformedJoinID = addCommit(repo,
                                     aliceAccount,
                                     "main",
                                     "{\"action\":\"join\",\"type\":\"member\",\"uri\":"
                                     "\"joinGarbage\"}");

    // Log the repository
    auto conversationCommits = repository->log();
    // Call the validation for all the commits
    bool allCommitsValidated = repository->validCommits(conversationCommits);
    // Check that the appropriate signal was called
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return isMalformed; }));
    // Check that the return value was false (i.e. malformed commit detected)
    CPPUNIT_ASSERT(!allCommitsValidated);
}

void
ConversationRepositoryTest::testMalformedCommitUri()
{
    // Register signals
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;

    // Variable to be set to true on detection of malformed commit
    bool isMalformed = false;

    // Signals for malformed commits in validCommits function
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::OnConversationError>(
            [&](const std::string& accountId,
                const std::string& conversationId,
                int code,
                auto malformedSpec) {
                if (code == EVALIDFETCH) {
                    isMalformed = true;
                }
                cv.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);

    // Get Alice's account
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    // Create a repository associated with Alice's account
    auto repository = ConversationRepository::createConversation(aliceAccount);
    // Assert that repository exists
    CPPUNIT_ASSERT(repository != nullptr);
    // Get the path to the conversation repository
    auto repoPath = fileutils::get_data_dir() / aliceAccount->getAccountID() / "conversations"
                    / repository->id();
    // Assert that the repository path is a directory
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));

    // Assert that the git repo was opened successfully
    git_repository* repo;
    CPPUNIT_ASSERT(git_repository_open(&repo, repoPath.c_str()) == 0);

    // Create malformed add and join commits with valid user InfoHash,
    // but with invalid "jami:" prefix
    std::string malformedID = "jami:" + aliceAccount->getUsername();
    std::string invitedPath = "invited/" + malformedID;

    CPPUNIT_ASSERT(!std::filesystem::exists(repoPath / invitedPath));

    // Simulate an invitation by creating the "invited/" directory with an empty file
    std::filesystem::create_directories(repoPath / "invited");
    std::ofstream(repoPath / invitedPath).close();
    CPPUNIT_ASSERT(std::filesystem::exists(repoPath / invitedPath));

    addAll(repo);

    auto malformedAddUserId = addCommit(
        repo,
        aliceAccount,
        "main",
        "{\"action\":\"add\",\"type\":\"member\",\"uri\":\"" + malformedID + "\"}"
    );

    auto malformedJoinUserID = addCommit(
        repo,
        aliceAccount,
        "main",
        "{\"action\":\"join\",\"type\":\"member\",\"uri\":\"" + malformedID + "\"}"
    );

    // Log the repository
    auto conversationCommits = repository->log();
    // Call the validation for all the commits
    bool allCommitsValidated = repository->validCommits(conversationCommits);

    // Check that the appropriate signal was called
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return isMalformed; }));
    // Check that the return value was false (i.e. malformed commit detected)
    CPPUNIT_ASSERT(!allCommitsValidated);
}

// A join without a previous add should be invalid
void
ConversationRepositoryTest::testInvalidJoin()
{
    // Register signals
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;

    // Variable to be set to true on detection of malformed commit
    bool isInvalid = false;

    // Signals for malformed commits in validCommits function
    confHandlers.insert(
        libjami::exportable_callback<libjami::ConversationSignal::OnConversationError>(
            [&](const std::string& accountId,
                const std::string& conversationId,
                int code,
                auto malformedSpec) {
                if (code == EVALIDFETCH) {
                    isInvalid = true;
                }
                cv.notify_one();
            }));
    libjami::registerSignalHandlers(confHandlers);

    // Get Alice's account
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    // Create a repository associated with Alice's account
    auto repository = ConversationRepository::createConversation(aliceAccount);
    // Assert that repository exists
    CPPUNIT_ASSERT(repository != nullptr);
    // Get the path to the conversation repository
    auto repoPath = fileutils::get_data_dir() / aliceAccount->getAccountID() / "conversations"
                    / repository->id();
    // Assert that the repository path is a directory
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));

    // Assert that the git repo was opened successfully
    git_repository* repo;
    CPPUNIT_ASSERT(git_repository_open(&repo, repoPath.c_str()) == 0);

    std::string userId = aliceAccount->getUsername();

    auto invalidJoinUserID = addCommit(
        repo,
        aliceAccount,
        "main",
        "{\"action\":\"join\",\"type\":\"member\",\"uri\":\"" + userId + "\"}"
    );

    // Log the repository
    auto conversationCommits = repository->log();
    // Call the validation for all the commits
    bool allCommitsValidated = repository->validCommits(conversationCommits);

    // Check that the appropriate signal was called
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return isInvalid; }));
    // Check that the return value was false (i.e. invalid commit detected)
    CPPUNIT_ASSERT(!allCommitsValidated);
}

=======
>>>>>>> BASE      (161cd0 Revwalk fixes - in progress)
} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::ConversationRepositoryTest::name())
