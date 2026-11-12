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
#include "gittransport.h"
#include "jamidht/conversation.h"
#include "jamidht/conversation_module.h"
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
#include <dhtnet/multiplexed_socket.h>

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <condition_variable>
#include <algorithm>
#include <array>
#include <string>
#include <fstream>
#include <streambuf>
#include <filesystem>
#include <thread>
#include <tuple>

using namespace std::string_literals;
using namespace std::literals::chrono_literals;
using namespace libjami::Account;

namespace jami {
namespace test {

class ConversationRepositoryTest : public CppUnit::TestFixture
{
public:
    ConversationRepositoryTest()
    {
        // Init daemon
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    }
    ~ConversationRepositoryTest() { libjami::fini(); }
    static std::string name() { return "ConversationRepository"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    std::string bobId;
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;

private:
    void testCreateRepository();
    void testAddSomeMessages();
    void testLogMessages();
    void testMerge();
    void testFFMerge();
    void testDiff();
    void testMergeProfileWithConflict();
    void testValidSignatureForCommit();
    void testInvalidSignatureForCommit();
    void testMalformedCommit();
    void testMalformedCommitUri();
    void testInvalidJoin();
    void testInvalidFile();
    void testMergeWithInvalidFile();
    void testCloneFailureDoesNotWipeExistingConversation();
    void testCommitIsNotBlockedByAStalledFetch();
    void testCreateDocumentRepository();
    void testDocumentCheckpointsAndAttachments();
    void testCheckpointInConversationRejected();
    void testCheckpointWithInvalidFileRejected();
    void testNonContentAddressedAttachmentRejected();
    void testCheckpointDeletingDeviceCertRejected();
    void testAttachmentInConversationRejected();
    void testMessageInDocumentRejected();
    void testRepackKeepsHistory();
    void testModePermissionsRejected();
    void testLocalPermissionDenied();
    void testRemoveOtherMemberRejected();
    void testUnknownCommitTypeInDocumentRejected();
    void testJoinLeavePermissions();
    void testMemberPermissions();
    void testFeedMemberActions();
    void testFeedFollowerCanRejoin();
    void testFeedJoinStillRequiresAuthorization();
    void testVoteResolutionHalfRejected();
    void testVoteResolutionMajorityAccepted();
    void testVoteResolutionCountsParentTree();
    void testApprovalThresholdParse();
    void testApprovalThresholdRequired();
    void testRoleChangePermissions();
    void testAdminVotesAccepted();
    void testInvalidVotesRejected();
    void testRoleActionsByMemberRejected();
    void testPromoteResolution();
    void testDemoteResolution();
    void testStepDown();
    void testDemoteLastAdminRejected();
    void testApprovalChange();
    void testApprovalResolutionWriteFailure();
    void testApprovalResolutionCommitFailure();
    void testApprovalResolutionStagingFailure();
    void testPendingVoteListing();
    void testStoredThresholdApplies();
    void testUnanimousThreshold();
    void testInvalidStoredThresholdRejectsVotes();
    void testBannedWinsOverAdmin();
    void testBannedWinsOverMember();
    void testMergeApprovalWithConflict();
    bool validBanResolution(std::size_t voterCount, bool dropSecondAdminFromWorkdir);
    // A conversation whose history is written by hand, with commits validated one at a time
    struct HandBuiltSwarm
    {
        std::shared_ptr<ConversationRepository> repository;
        std::filesystem::path path;
        GitRepository repo;
    };
    // Alice's conversation with more admins, members and a stored approval threshold
    HandBuiltSwarm handBuiltSwarm(const std::vector<std::string>& extraAdmins,
                                  const std::vector<std::string>& members,
                                  const std::string& approvalThreshold = "");
    std::string fileContent(const HandBuiltSwarm& swarm, const std::string& path);
    // Write and stage a file of the working directory
    void writeFile(HandBuiltSwarm& swarm, const std::string& path, const std::string& content = "");
    // Delete and unstage a file of the working directory
    void removeFile(HandBuiltSwarm& swarm, const std::string& path);
    void moveFile(HandBuiltSwarm& swarm, const std::string& from, const std::string& to);
    // Commit what is staged on main without validating it
    std::string commitRaw(HandBuiltSwarm& swarm, const CommitMessage& message);
    // Commit what is staged by Alice and validate that commit alone. Unless keepOnMain, the commit
    // goes to a side branch and the index and the working directory return to main
    bool commitValidates(HandBuiltSwarm& swarm, const CommitMessage& message, bool keepOnMain);
    MemberRole roleOf(const HandBuiltSwarm& swarm, const std::string& uri);
    std::string addCommit(git_repository* repo,
                          const std::shared_ptr<JamiAccount> account,
                          const std::string& branch,
                          const std::string& commit_msg);
    std::string addMergeCommit(git_repository* repo,
                               const std::shared_ptr<JamiAccount> account,
                               const std::string& wanted_ref);
    void addAll(git_repository* repo);
    bool merge_in_main(const std::shared_ptr<JamiAccount> account, git_repository* repo, const std::string& commit_ref);
    CPPUNIT_TEST_SUITE(ConversationRepositoryTest);
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
    CPPUNIT_TEST(testInvalidFile);               // Passes
    CPPUNIT_TEST(testMergeWithInvalidFile);      // Passes
    CPPUNIT_TEST(testCloneFailureDoesNotWipeExistingConversation);
    CPPUNIT_TEST(testCommitIsNotBlockedByAStalledFetch);
    CPPUNIT_TEST(testCreateDocumentRepository);
    CPPUNIT_TEST(testDocumentCheckpointsAndAttachments);
    CPPUNIT_TEST(testCheckpointInConversationRejected);
    CPPUNIT_TEST(testCheckpointWithInvalidFileRejected);
    CPPUNIT_TEST(testNonContentAddressedAttachmentRejected);
    CPPUNIT_TEST(testCheckpointDeletingDeviceCertRejected);
    CPPUNIT_TEST(testAttachmentInConversationRejected);
    CPPUNIT_TEST(testMessageInDocumentRejected);
    CPPUNIT_TEST(testRepackKeepsHistory);
    CPPUNIT_TEST(testModePermissionsRejected);
    CPPUNIT_TEST(testLocalPermissionDenied);
    CPPUNIT_TEST(testRemoveOtherMemberRejected);
    CPPUNIT_TEST(testUnknownCommitTypeInDocumentRejected);
    CPPUNIT_TEST(testJoinLeavePermissions);
    CPPUNIT_TEST(testMemberPermissions);
    CPPUNIT_TEST(testFeedMemberActions);
    CPPUNIT_TEST(testFeedFollowerCanRejoin);
    CPPUNIT_TEST(testFeedJoinStillRequiresAuthorization);
    CPPUNIT_TEST(testVoteResolutionHalfRejected);
    CPPUNIT_TEST(testVoteResolutionMajorityAccepted);
    CPPUNIT_TEST(testVoteResolutionCountsParentTree);
    CPPUNIT_TEST(testApprovalThresholdParse);
    CPPUNIT_TEST(testApprovalThresholdRequired);
    CPPUNIT_TEST(testRoleChangePermissions);
    CPPUNIT_TEST(testAdminVotesAccepted);
    CPPUNIT_TEST(testInvalidVotesRejected);
    CPPUNIT_TEST(testRoleActionsByMemberRejected);
    CPPUNIT_TEST(testPromoteResolution);
    CPPUNIT_TEST(testDemoteResolution);
    CPPUNIT_TEST(testStepDown);
    CPPUNIT_TEST(testDemoteLastAdminRejected);
    CPPUNIT_TEST(testApprovalChange);
    CPPUNIT_TEST(testApprovalResolutionWriteFailure);
    CPPUNIT_TEST(testApprovalResolutionCommitFailure);
    CPPUNIT_TEST(testApprovalResolutionStagingFailure);
    CPPUNIT_TEST(testPendingVoteListing);
    CPPUNIT_TEST(testStoredThresholdApplies);
    CPPUNIT_TEST(testUnanimousThreshold);
    CPPUNIT_TEST(testInvalidStoredThresholdRejectsVotes);
    CPPUNIT_TEST(testBannedWinsOverAdmin);
    CPPUNIT_TEST(testBannedWinsOverMember);
    CPPUNIT_TEST(testMergeApprovalWithConflict);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ConversationRepositoryTest, ConversationRepositoryTest::name());

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
    auto repoPath = fileutils::get_data_dir() / aliceAccount->getAccountID() / "conversations" / repository->id();
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
    std::string deviceCrtStr((std::istreambuf_iterator<char>(crt)), std::istreambuf_iterator<char>());

    CPPUNIT_ASSERT(deviceCrtStr == deviceCert);
}

void
ConversationRepositoryTest::testAddSomeMessages()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto repository = ConversationRepository::createConversation(aliceAccount);

    auto id1 = repository->commitMessage(R"({"body":"Commit 1","type":"text/plain"})");
    auto id2 = repository->commitMessage(R"({"body":"Commit 2","type":"text/plain"})");
    auto id3 = repository->commitMessage(R"({"body":"Commit 3","type":"text/plain"})");

    auto messages = repository->log();
    CPPUNIT_ASSERT(messages.size() == 4 /* 3 + initial */);
    CPPUNIT_ASSERT(messages[0].id == id3);
    CPPUNIT_ASSERT(messages[0].parents.front() == id2);
    CPPUNIT_ASSERT(messages[0].commitMsg.toString() == R"({"body":"Commit 3","type":"text/plain"})");
    CPPUNIT_ASSERT(messages[0].author.name == messages[3].author.name);
    CPPUNIT_ASSERT(messages[0].author.email == messages[3].author.email);
    CPPUNIT_ASSERT(messages[1].id == id2);
    CPPUNIT_ASSERT(messages[1].parents.front() == id1);
    CPPUNIT_ASSERT(messages[1].commitMsg.toString() == R"({"body":"Commit 2","type":"text/plain"})");
    CPPUNIT_ASSERT(messages[1].author.name == messages[3].author.name);
    CPPUNIT_ASSERT(messages[1].author.email == messages[3].author.email);
    CPPUNIT_ASSERT(messages[2].id == id1);
    CPPUNIT_ASSERT(messages[2].commitMsg.toString() == R"({"body":"Commit 1","type":"text/plain"})");
    CPPUNIT_ASSERT(messages[2].author.name == messages[3].author.name);
    CPPUNIT_ASSERT(messages[2].author.email == messages[3].author.email);
    CPPUNIT_ASSERT(messages[2].parents.front() == repository->id());
    // Check sig
    CPPUNIT_ASSERT(aliceAccount->identity().second->getPublicKey().checkSignature(messages[0].signed_content,
                                                                                  messages[0].signature));
    CPPUNIT_ASSERT(aliceAccount->identity().second->getPublicKey().checkSignature(messages[1].signed_content,
                                                                                  messages[1].signature));
    CPPUNIT_ASSERT(aliceAccount->identity().second->getPublicKey().checkSignature(messages[2].signed_content,
                                                                                  messages[2].signature));
}

void
ConversationRepositoryTest::testLogMessages()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto repository = ConversationRepository::createConversation(aliceAccount);

    auto id1 = repository->commitMessage(R"({"body":"Commit 1","type":"text/plain"})");
    auto id2 = repository->commitMessage(R"({"body":"Commit 2","type":"text/plain"})");
    auto id3 = repository->commitMessage(R"({"body":"Commit 3","type":"text/plain"})");

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

std::string
ConversationRepositoryTest::addCommit(git_repository* repo,
                                      const std::shared_ptr<JamiAccount> account,
                                      const std::string& branch,
                                      const std::string& commit_msg)
{
    auto deviceId = DeviceId(std::string(account->currentDeviceId()));
    auto name = account->getDisplayName();
    if (name.empty())
        name = deviceId.toString();

    git_signature* sig_ptr = nullptr;
    // Sign commit's buffer
    if (git_signature_new(&sig_ptr, name.c_str(), deviceId.to_c_str(), std::time(nullptr), 0) < 0) {
        JAMI_ERROR("Unable to create a commit signature.");
        return {};
    }
    GitSignature sig {sig_ptr};

    // Retrieve current HEAD
    git_oid commit_id;
    if (git_reference_name_to_id(&commit_id, repo, "HEAD") < 0) {
        JAMI_ERROR("Unable to get reference for HEAD");
        return {};
    }

    git_commit* head_ptr = nullptr;
    if (git_commit_lookup(&head_ptr, repo, &commit_id) < 0) {
        JAMI_ERROR("Unable to look up HEAD commit");
        return {};
    }
    GitCommit head_commit {head_ptr};

    // Retrieve current index
    git_index* index_ptr = nullptr;
    if (git_repository_index(&index_ptr, repo) < 0) {
        JAMI_ERROR("Unable to open repository index");
        return {};
    }
    GitIndex index {index_ptr};

    git_oid tree_id;
    if (git_index_write_tree(&tree_id, index.get()) < 0) {
        JAMI_ERROR("Unable to write initial tree from index");
        return {};
    }

    git_tree* tree_ptr = nullptr;
    if (git_tree_lookup(&tree_ptr, repo, &tree_id) < 0) {
        JAMI_ERROR("Unable to look up initial tree");
        return {};
    }
    GitTree tree = GitTree(tree_ptr);

    git_buf to_sign = {};
#if LIBGIT2_VER_MAJOR == 1 && LIBGIT2_VER_MINOR == 8 \
    && (LIBGIT2_VER_REVISION == 0 || LIBGIT2_VER_REVISION == 1 || LIBGIT2_VER_REVISION == 3)
    git_commit* const head_ref[1] = {head_commit.get()};
#else
    const git_commit* head_ref[1] = {head_commit.get()};
#endif
    if (git_commit_create_buffer(
            &to_sign, repo, sig.get(), sig.get(), nullptr, commit_msg.c_str(), tree.get(), 1, &head_ref[0])
        < 0) {
        JAMI_ERROR("Unable to create commit buffer");
        return {};
    }

    // git commit -S
    auto to_sign_vec = std::vector<uint8_t>(to_sign.ptr, to_sign.ptr + to_sign.size);
    auto signed_buf = account->identity().first->sign(to_sign_vec);
    std::string signed_str = base64::encode(signed_buf);
    if (git_commit_create_with_signature(&commit_id, repo, to_sign.ptr, signed_str.c_str(), "signature") < 0) {
        JAMI_ERROR("Unable to sign commit");
        return {};
    }

    auto commit_str = git_oid_tostr_s(&commit_id);
    if (commit_str) {
        JAMI_LOG("New commit added with id: {}", commit_str);
        // Move commit to main branch
        git_reference* ref_ptr = nullptr;
        std::string branch_name = "refs/heads/" + branch;
        if (git_reference_create(&ref_ptr, repo, branch_name.c_str(), &commit_id, true, nullptr) < 0) {
            JAMI_WARNING("Unable to move commit to main");
        }
        git_reference_free(ref_ptr);
    }
    return commit_str ? commit_str : "";
}

std::string
ConversationRepositoryTest::addMergeCommit(git_repository* repo,
                                           const std::shared_ptr<JamiAccount> account,
                                           const std::string& wanted_ref)
{
    auto deviceId = DeviceId(std::string(account->currentDeviceId()));
    auto name = account->getDisplayName();
    if (name.empty())
        name = deviceId.toString();

    git_signature* sig_ptr = nullptr;
    if (git_signature_new(&sig_ptr, name.c_str(), deviceId.to_c_str(), std::time(nullptr), 0) < 0) {
        return {};
    }
    GitSignature sig {sig_ptr};

    // Get current HEAD (parent 1)
    git_oid head_oid;
    if (git_reference_name_to_id(&head_oid, repo, "HEAD") < 0) {
        JAMI_ERROR("Unable to get HEAD commit");
        return "";
    }

    git_commit* head_ptr = nullptr;
    if (git_commit_lookup(&head_ptr, repo, &head_oid) < 0) {
        JAMI_ERROR("Unable to look up HEAD commit");
        return "";
    }

    GitCommit head_commit {head_ptr};

    // Get wanted commit (parent 2)
    git_oid wanted_oid;
    if (git_oid_fromstr(&wanted_oid, wanted_ref.c_str()) < 0) {
        JAMI_ERROR("Unable to get wanted commit");
        return "";
    }

    git_commit* wanted_ptr = nullptr;
    if (git_commit_lookup(&wanted_ptr, repo, &wanted_oid) < 0) {
        JAMI_ERROR("Unable to look up wanted commit");
        return "";
    }
    GitCommit wanted_commit {wanted_ptr};

    // Get current index and tree (same as addCommit)
    git_index* index_ptr = nullptr;
    if (git_repository_index(&index_ptr, repo) < 0) {
        JAMI_ERROR("Unable to get repository index");
        return {};
    }
    GitIndex index {index_ptr};

    git_oid tree_id;
    if (git_index_write_tree(&tree_id, index.get()) < 0) {
        JAMI_ERROR("Unable to write tree from index");
        return {};
    }
    git_tree* tree_ptr = nullptr;
    if (git_tree_lookup(&tree_ptr, repo, &tree_id) < 0) {
        JAMI_ERROR("Unable to look up tree");
        return {};
    }
    GitTree tree = GitTree(tree_ptr);

    // Create merge message
    auto commitMsg = fmt::format("Merge commit '{}'", wanted_ref);

    git_buf to_sign = {};
#if LIBGIT2_VER_MAJOR == 1 && LIBGIT2_VER_MINOR == 8 \
    && (LIBGIT2_VER_REVISION == 0 || LIBGIT2_VER_REVISION == 1 || LIBGIT2_VER_REVISION == 3)
    git_commit* const parents[2] = {head_commit.get(), wanted_commit.get()};
#else
    const git_commit* parents[2] = {head_commit.get(), wanted_commit.get()};
#endif
    if (git_commit_create_buffer(
            &to_sign, repo, sig.get(), sig.get(), nullptr, commitMsg.c_str(), tree.get(), 2, &parents[0])
        < 0) {
        return {};
    }

    auto to_sign_vec = std::vector<uint8_t>(to_sign.ptr, to_sign.ptr + to_sign.size);
    auto signed_buf = account->identity().first->sign(to_sign_vec);
    std::string signed_str = base64::encode(signed_buf);
    git_oid commit_id;
    if (git_commit_create_with_signature(&commit_id, repo, to_sign.ptr, signed_str.c_str(), "signature") < 0) {
        git_buf_dispose(&to_sign);
        return {};
    }
    git_buf_dispose(&to_sign);

    auto commit_str = git_oid_tostr_s(&commit_id);
    if (commit_str) {
        // Move to main branch
        git_reference* ref_ptr = nullptr;
        if (git_reference_create(&ref_ptr, repo, "refs/heads/main", &commit_id, true, nullptr) < 0) {
            JAMI_WARNING("Unable to move commit to main");
            return {};
        }
        git_reference_free(ref_ptr);
    }
    return commit_str ? commit_str : "";
}

void
ConversationRepositoryTest::addAll(git_repository* repo)
{
    // git add -A
    git_index* index_ptr = nullptr;
    if (git_repository_index(&index_ptr, repo) < 0)
        return;
    GitIndex index {index_ptr};
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
    auto repoPath = fileutils::get_data_dir() / aliceAccount->getAccountID() / "conversations" / repository->id();
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));

    // Assert that first commit is signed by alice
    git_repository* repo;
    CPPUNIT_ASSERT(git_repository_open(&repo, repoPath.c_str()) == 0);
    auto id1 = addCommit(repo, aliceAccount, "main", R"({"body":"Commit 1","type":"text/plain"})");

    git_reference* ref = nullptr;
    git_commit* commit = nullptr;
    git_oid commit_id;
    git_oid_fromstr(&commit_id, repository->id().c_str());
    git_commit_lookup(&commit, repo, &commit_id);
    git_branch_create(&ref, repo, "to_merge", commit, false);
    git_reference_free(ref);
    git_repository_set_head(repo, "refs/heads/to_merge");

    auto id2 = addCommit(repo, aliceAccount, "to_merge", R"({"body":"Commit 2","type":"text/plain"})");
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
    auto repoPath = fileutils::get_data_dir() / aliceAccount->getAccountID() / "conversations" / repository->id();
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));

    // Assert that first commit is signed by alice
    git_repository* repo;
    CPPUNIT_ASSERT(git_repository_open(&repo, repoPath.c_str()) == 0);
    auto id1 = addCommit(repo, aliceAccount, "main", R"({"body":"Commit 1","type":"text/plain"})");

    git_reference* ref = nullptr;
    git_commit* commit = nullptr;
    git_oid commit_id;
    git_oid_fromstr(&commit_id, id1.c_str());
    git_commit_lookup(&commit, repo, &commit_id);
    git_branch_create(&ref, repo, "to_merge", commit, false);
    git_reference_free(ref);
    git_repository_set_head(repo, "refs/heads/to_merge");

    auto id2 = addCommit(repo, aliceAccount, "to_merge", R"({"body":"Commit 2","type":"text/plain"})");
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

    auto id1 = repository->commitMessage(CommitMessage::text("Commit 1").toString());
    auto id2 = repository->commitMessage(CommitMessage::text("Commit 2").toString());
    auto id3 = repository->commitMessage(CommitMessage::text("Commit 3").toString());

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
    auto repoPath = fileutils::get_data_dir() / aliceAccount->getAccountID() / "conversations" / repository->id();
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
    auto id1 = addCommit(repo, aliceAccount, "main", R"({"type":"application/update-profile"})");
    profile = std::ofstream(repoPath / "profile.vcf");
    if (profile.is_open()) {
        profile << "TITLE: SWARM\n";
        profile << "SUBTITLE: New description\n";
        profile << "AVATAR: BASE64\n";
        profile.close();
    }
    addAll(repo);
    auto id2 = addCommit(repo, aliceAccount, "main", R"({"type":"application/update-profile"})");

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
    auto id3 = addCommit(repo, aliceAccount, "to_merge", R"({"type":"application/update-profile"})");

    // This will create a merge commit
    repository->merge(id3);
    CPPUNIT_ASSERT(repository->log().size() == 5 /* Initial, add, modify 1, modify 2, merge */);
}

void
ConversationRepositoryTest::testValidSignatureForCommit()
{
    // Get Alice's account
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    // Create a repository associated with Alice's account
    auto repository = ConversationRepository::createConversation(aliceAccount);
    // Assert the repository exists
    CPPUNIT_ASSERT(repository != nullptr);
    // Get the path to the conversation repository
    auto repoPath = fileutils::get_data_dir() / aliceAccount->getAccountID() / "conversations" / repository->id();
    // Assert that the repository path is a directory
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));

    // Assert that the git repo was opened successfully
    git_repository* repo;
    CPPUNIT_ASSERT(git_repository_open(&repo, repoPath.c_str()) == 0);

    // Get the initial commit
    git_oid commit_id;
    CPPUNIT_ASSERT(git_reference_name_to_id(&commit_id, repo, "HEAD") == 0);

    // Get the commit object
    git_commit* commit_ptr = nullptr;
    CPPUNIT_ASSERT(git_commit_lookup(&commit_ptr, repo, &commit_id) == 0);

    // Extract the author email from the commit
    const git_signature* author = git_commit_author(commit_ptr);
    std::string userDevice = author->email;

    // Extract the signature and signature data of commit_id
    git_buf extracted_sig = {}, extracted_sig_data = {};
    // Extract the signature block and signature content from the commit
    git_commit_extract_signature(&extracted_sig, &extracted_sig_data, repo, &commit_id, "signature");
    // Verify the signature of the commit
    CPPUNIT_ASSERT(
        repository->isValidUserAtCommit(userDevice, git_oid_tostr_s(&commit_id), extracted_sig, extracted_sig_data));

    // Free git objects
    git_commit_free(commit_ptr);
    git_repository_free(repo);
}

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
    auto repoPath = fileutils::get_data_dir() / aliceAccount->getAccountID() / "conversations" / repository->id();
    // Assert that the repository path is a directory
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));

    // Assert that the git repo was opened successfully
    git_repository* repo;
    CPPUNIT_ASSERT(git_repository_open(&repo, repoPath.c_str()) == 0);

    // Make a malicious commit that disguises itself as being sent from Alice
    // Retrieve current index
    git_index* index_ptr = nullptr;
    CPPUNIT_ASSERT(git_repository_index(&index_ptr, repo) == 0);
    GitIndex index {index_ptr};

    // Write the index to a new tree
    git_oid tree_id;
    // Assert that the tree was written successfully
    CPPUNIT_ASSERT(git_index_write_tree(&tree_id, index.get()) == 0);

    // Lookup the newly created tree
    git_tree* tree_ptr = nullptr;
    CPPUNIT_ASSERT(git_tree_lookup(&tree_ptr, repo, &tree_id) == 0);
    GitTree tree = GitTree(tree_ptr);

    // Create a commit object
    git_oid commit_id;
    CPPUNIT_ASSERT(git_reference_name_to_id(&commit_id, repo, "HEAD") == 0);

    // Lookup the HEAD commit
    git_commit* head_ptr = nullptr;
    CPPUNIT_ASSERT(git_commit_lookup(&head_ptr, repo, &commit_id) == 0);
    GitCommit head_commit {head_ptr};

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
    CPPUNIT_ASSERT(git_commit_create_buffer(&to_sign,
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
    CPPUNIT_ASSERT(git_commit_create_with_signature(&commit_id, repo, to_sign.ptr, signed_str.c_str(), "signature")
                   == 0);

    // Dispose the signature buffer
    git_buf_dispose(&to_sign);

    // Move commit to main branch
    git_reference* ref_ptr = nullptr;
    CPPUNIT_ASSERT((git_reference_create(&ref_ptr, repo, "refs/heads/main", &commit_id, true, nullptr) == 0));

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
    CPPUNIT_ASSERT(
        !repository->isValidUserAtCommit(userDevice, git_oid_tostr_s(&commit_id), extracted_sig, extracted_sig_data));

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
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::OnConversationError>(
        [&](const std::string& /*accountId*/,
            const std::string& /*conversationId*/,
            int code,
            const std::string& /*malformedSpec*/) {
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
    auto repoPath = fileutils::get_data_dir() / aliceAccount->getAccountID() / "conversations" / repository->id();
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
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::OnConversationError>(
        [&](const std::string& /*accountId*/,
            const std::string& /*conversationId*/,
            int code,
            const std::string& /*malformedSpec*/) {
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
    auto repoPath = fileutils::get_data_dir() / aliceAccount->getAccountID() / "conversations" / repository->id();
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

    auto malformedAddUserId = addCommit(repo,
                                        aliceAccount,
                                        "main",
                                        "{\"action\":\"add\",\"type\":\"member\",\"uri\":\"" + malformedID + "\"}");
    CPPUNIT_ASSERT(!malformedAddUserId.empty());

    auto malformedJoinUserID = addCommit(repo,
                                         aliceAccount,
                                         "main",
                                         "{\"action\":\"join\",\"type\":\"member\",\"uri\":\"" + malformedID + "\"}");
    CPPUNIT_ASSERT(!malformedJoinUserID.empty());

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
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::OnConversationError>(
        [&](const std::string& /*accountId*/,
            const std::string& /*conversationId*/,
            int code,
            const std::string& /*malformedSpec*/) {
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
    // Assert that the repository exists
    CPPUNIT_ASSERT(repository != nullptr);
    // Get the path to the conversation repository
    auto repoPath = fileutils::get_data_dir() / aliceAccount->getAccountID() / "conversations" / repository->id();
    // Assert that the repository path is a directory
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));

    // Assert that the git repo was opened successfully
    git_repository* repo;
    CPPUNIT_ASSERT(git_repository_open(&repo, repoPath.c_str()) == 0);

    std::string userId = aliceAccount->getUsername();

    auto invalidJoinUserID = addCommit(repo,
                                       aliceAccount,
                                       "main",
                                       "{\"action\":\"join\",\"type\":\"member\",\"uri\":\"" + userId + "\"}");
    CPPUNIT_ASSERT(!invalidJoinUserID.empty());

    // Log the repository
    auto conversationCommits = repository->log();
    // Call the validation for all the commits
    bool allCommitsValidated = repository->validCommits(conversationCommits);

    // Check that the appropriate signal was called
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return isInvalid; }));
    // Check that the return value was false (i.e. invalid commit detected)
    CPPUNIT_ASSERT(!allCommitsValidated);
}

void
ConversationRepositoryTest::testInvalidFile()
{
    // Register signals
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;

    // Variable to be set to true on detection of malformed commit
    bool isInvalid = false;

    // Signals for malformed commits in validCommits function
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::OnConversationError>(
        [&](const std::string& /*accountId*/,
            const std::string& /*conversationId*/,
            int code,
            const std::string& /*malformedSpec*/) {
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
    auto repoPath = fileutils::get_data_dir() / aliceAccount->getAccountID() / "conversations" / repository->id();
    // Assert that the repository path is a directory
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));

    // Assert that the git repo was opened successfully
    git_repository* repo;
    CPPUNIT_ASSERT(git_repository_open(&repo, repoPath.c_str()) == 0);

    std::string userId = aliceAccount->getUsername();

    auto invalidFile = repoPath / "invalidFile.txt";
    std::ofstream(invalidFile).close();
    addAll(repo);

    auto invalidFileCommitID = addCommit(repo,
                                         aliceAccount,
                                         "main",
                                         "{\"type\":\"other\",\"note\":\"Add invalid file\"}");

    // Log the repository
    auto conversationCommits = repository->log();
    // Call the validation for all the commits
    bool allCommitsValidated = repository->validCommits(conversationCommits);

    // Check that the appropriate signal was called
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return isInvalid; }));
    // Check that the return value was false (i.e. invalid commit detected)
    CPPUNIT_ASSERT(!allCommitsValidated);
}

void
ConversationRepositoryTest::testMergeWithInvalidFile()
{
    // Register signals
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;

    // Variable to be set to true on detection of malformed commit
    bool isInvalid = false;

    // Get member accounts
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();

    bobAccount->addContact(aliceUri);

    auto bobRequestReceived = false;

    std::vector<libjami::SwarmMessage> aliceMessages = {};

    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::OnConversationError>(
        [&](const std::string& /*accountId*/,
            const std::string& /*conversationId*/,
            int code,
            const std::string& /*malformedSpec*/) {
            if (code == EVALIDFETCH) {
                isInvalid = true;
            }
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::ConversationRequestReceived>(
        [&](const std::string& accountId,
            const std::string& /* conversationId */,
            std::map<std::string, std::string> /*metadatas*/) {
            if (accountId == bobId) {
                bobRequestReceived = true;
            }
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::SwarmMessageReceived>(
        [&](const std::string& accountId, const std::string& /* conversationId */, libjami::SwarmMessage message) {
            if (accountId == aliceId) {
                aliceMessages.emplace_back(message);
            }
            cv.notify_one();
        }));

    libjami::registerSignalHandlers(confHandlers);

    auto convId = libjami::startConversation(aliceId);

    // Alice adds Bob to the conversation
    libjami::addConversationMember(aliceId, convId, bobUri);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return bobRequestReceived; }));

    auto aliceMsgSize = aliceMessages.size();
    libjami::acceptConversationRequest(bobId, convId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&]() { return aliceMsgSize + 1 == aliceMessages.size(); }));

    auto convMembers = libjami::getConversationMembers(aliceId, convId);
    CPPUNIT_ASSERT(convMembers.size() == 2);

    // Alice and Bob exchange messages
    std::string msgId1 = "", msgId2 = "";
    aliceAccount->convModule()->sendMessage(convId, "1"s, "", true, {}, [&](bool, std::string commitId) {
        msgId1 = commitId;
        cv.notify_one();
    });
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return !msgId1.empty(); }));
    bobAccount->convModule()->sendMessage(convId, "2"s, "", true, {}, [&](bool, std::string commitId) {
        msgId2 = commitId;
        cv.notify_one();
    });
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return !msgId2.empty(); }));

    // Retrieve the repository associated with Alice's account
    auto aliceRepository = std::make_unique<ConversationRepository>(aliceAccount, convId);
    // Assert that the repository exists
    CPPUNIT_ASSERT(aliceRepository != nullptr);
    // Get the path to the conversation repository
    auto aliceRepoPath = fileutils::get_data_dir() / aliceAccount->getAccountID() / "conversations"
                         / aliceRepository->id();
    // Assert that the repository path is a directory
    CPPUNIT_ASSERT(std::filesystem::is_directory(aliceRepoPath));

    // Assert that Alice's git repo was opened successfully
    git_repository* aliceRepo;
    CPPUNIT_ASSERT(git_repository_open(&aliceRepo, aliceRepoPath.c_str()) == 0);

    // Bob adds an unwanted file
    auto bobRepository = std::make_unique<ConversationRepository>(bobAccount, convId);

    auto bobRepoPath = fileutils::get_data_dir() / bobAccount->getAccountID() / "conversations" / bobRepository->id();
    CPPUNIT_ASSERT(std::filesystem::is_directory(bobRepoPath));

    git_repository* bobRepo;
    CPPUNIT_ASSERT(git_repository_open(&bobRepo, bobRepoPath.c_str()) == 0);

    auto invalidBobFile = bobRepoPath / "invalidBobFile.txt";
    std::ofstream(invalidBobFile).close();
    addAll(bobRepo);

    // Add a fake merge commit associated with the invalid file
    auto invalidBobMergeCommitId = addMergeCommit(bobRepo, bobAccount, msgId2);

    // Log Bob's repository
    auto bobCommits = bobRepository->log();

    bool allCommitsValidated = bobRepository->validCommits(bobCommits);

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return isInvalid; }));
    CPPUNIT_ASSERT(!allCommitsValidated);
}

void
ConversationRepositoryTest::testCloneFailureDoesNotWipeExistingConversation()
{
    // Regression test: a failed clone must NOT wipe an already-present
    // conversation directory at the destination. Previously
    // ConversationRepository::cloneConversation() unconditionally removed
    // the target directory before attempting the clone, which meant any
    // clone failure (network error, oversized pack, invalid remote, failed
    // validation, ...) resulted in permanent loss of the previously-valid
    // local conversation - opening the door to later syncing an empty
    // conversation on top of peers.
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);

    // Create a real, valid conversation locally.
    auto repository = ConversationRepository::createConversation(aliceAccount);
    CPPUNIT_ASSERT(repository != nullptr);
    const auto convId = repository->id();
    const auto repoPath = fileutils::get_data_dir() / aliceAccount->getAccountID() / "conversations" / convId;
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));

    // Record the head so we can verify the original history survives the
    // failed clone attempt.
    const auto originalHead = repository->getHead();
    CPPUNIT_ASSERT(!originalHead.empty());

    // Drop a sentinel file inside the repo workdir; it must still be there
    // after the failed clone.
    const auto sentinel = repoPath / "sentinel.precious";
    {
        std::ofstream(sentinel) << "do-not-delete";
    }
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(sentinel));

    // Release the repository handle so libgit2 isn't holding the workdir
    // open while we trigger the clone path.
    repository.reset();

    // Force clone to resolve to a local file:// URL that does not exist so
    // git_clone() fails fast without needing any network or peer setup.
    const bool savedFlag = ConversationRepository::FETCH_FROM_LOCAL_REPOS;
    ConversationRepository::FETCH_FROM_LOCAL_REPOS = true;

    auto [clonedRepo, commits] = ConversationRepository::cloneConversation(aliceAccount,
                                                                           "device-that-does-not-exist",
                                                                           convId);

    ConversationRepository::FETCH_FROM_LOCAL_REPOS = savedFlag;

    // Clone MUST have failed - the source does not exist.
    CPPUNIT_ASSERT(clonedRepo == nullptr);
    CPPUNIT_ASSERT(commits.empty());

    // The pre-existing conversation directory, the sentinel file, and the
    // original git history MUST still be intact. This is the behaviour the
    // fix guarantees; without it, the directory and its contents are gone.
    CPPUNIT_ASSERT_MESSAGE("Pre-existing conversation directory was wiped by a failed clone",
                           std::filesystem::is_directory(repoPath));
    CPPUNIT_ASSERT_MESSAGE("Sentinel file inside the pre-existing conversation was wiped",
                           std::filesystem::is_regular_file(sentinel));

    // Re-open the repository and confirm the git head is unchanged.
    auto reopened = std::make_unique<ConversationRepository>(aliceAccount, convId);
    CPPUNIT_ASSERT(reopened != nullptr);
    CPPUNIT_ASSERT_EQUAL(originalHead, reopened->getHead());
}

// A peer that stops talking without hanging up leaves a fetch waiting inside
// the git transport. That wait must not take the whole repository with it: the
// user is still typing, and their messages have to reach git.
void
ConversationRepositoryTest::testCommitIsNotBlockedByAStalledFetch()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto convId = libjami::startConversation(aliceId);

    std::shared_ptr<Conversation> conv;
    for (auto i = 0; i < 100 && !conv; ++i) {
        conv = aliceAccount->convModule()->getConversation(convId);
        if (!conv)
            std::this_thread::sleep_for(50ms);
    }
    CPPUNIT_ASSERT(conv);

    // Shared, not captured by reference: the stalled fetch is still inside the
    // hook when an assertion unwinds this frame, so the state it waits on has to
    // outlive the frame.
    struct Stall
    {
        std::mutex mtx;
        std::condition_variable cv;
        bool stalling = false, release = false, committed = false, pulled = false;
    };
    auto stall = std::make_shared<Stall>();

    // Park the fetch inside the transport, exactly where a quiet peer would
    // park it, and keep it there until this test lets go. The guard lets go on
    // every exit from here, including a failed assertion.
    struct Release
    {
        std::shared_ptr<Stall> stall;
        ~Release()
        {
            P2P_STALL_HOOK.store(nullptr);
            std::lock_guard lk {stall->mtx};
            stall->release = true;
            stall->cv.notify_all();
        }
    } releaseOnExit {stall};

    P2P_STALL_HOOK.store(std::make_shared<P2PStallHook>([stall, convId](std::string_view url) {
        if (url.find(convId) == std::string_view::npos)
            return;
        std::unique_lock lk {stall->mtx};
        stall->stalling = true;
        stall->cv.notify_all();
        stall->cv.wait(lk, [&] { return stall->release; });
    }));

    DeviceId quiet {std::string(64, 'b')};
    conv->addGitSocket(quiet,
                       std::make_shared<dhtnet::ChannelSocket>(std::weak_ptr<dhtnet::MultiplexedSocket>(),
                                                               "git://quiet/" + convId,
                                                               1));
    conv->pull(quiet.toString(), [stall](bool) {
        std::lock_guard lk {stall->mtx};
        stall->pulled = true;
        stall->cv.notify_all();
    });

    {
        std::unique_lock lk {stall->mtx};
        CPPUNIT_ASSERT(stall->cv.wait_for(lk, 30s, [&] { return stall->stalling; }));
    }

    CommitMessage message;
    message.type = "text/plain";
    message.body = "hello";
    conv->createCommit(std::move(message), {}, [stall](bool ok, const std::string&) {
        std::lock_guard lk {stall->mtx};
        stall->committed = ok;
        stall->cv.notify_all();
    });

    bool done = false;
    {
        std::unique_lock lk {stall->mtx};
        done = stall->cv.wait_for(lk, 10s, [&] { return stall->committed; });
        // Let the fetch go and wait for it, so the next test starts on a
        // conversation that has no operation left running on it.
        stall->release = true;
        stall->cv.notify_all();
        stall->cv.wait_for(lk, 30s, [&] { return stall->pulled; });
    }
    CPPUNIT_ASSERT_MESSAGE("A commit had to wait for a stalled fetch", done);
}

// A collaborative document repository is a conversation repository in the
// DOCUMENT mode: same layout, same membership files, plus the id of the
// conversation it was announced in and its media type in the initial commit.
void
ConversationRepositoryTest::testCreateDocumentRepository()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceDeviceId = DeviceId(std::string(aliceAccount->currentDeviceId()));
    auto uri = aliceAccount->getUsername();

    auto repository = ConversationRepository::createDocument(aliceAccount, "some-conversation-id", "text/html");
    CPPUNIT_ASSERT(repository != nullptr);
    auto repoPath = fileutils::get_data_dir() / aliceAccount->getAccountID() / "conversations" / repository->id();
    CPPUNIT_ASSERT(std::filesystem::is_directory(repoPath));

    CPPUNIT_ASSERT(repository->mode() == ConversationMode::DOCUMENT);
    CPPUNIT_ASSERT_EQUAL("some-conversation-id"s, repository->parentConversationId());
    CPPUNIT_ASSERT_EQUAL("text/html"s, repository->documentMimeType());

    // Same membership layout as a conversation: the creator is the only admin
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(repoPath / "admins" / (uri + ".crt")));
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(repoPath / "devices" / (aliceDeviceId.toString() + ".crt")));

    // All commits (the initial one) pass validation
    CPPUNIT_ASSERT(repository->validCommits(repository->log()));
}

void
ConversationRepositoryTest::testDocumentCheckpointsAndAttachments()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto repository = ConversationRepository::createDocument(aliceAccount, "some-conversation-id", "text/html");
    CPPUNIT_ASSERT(repository != nullptr);

    // A checkpoint carries base64 CRDT updates in its message and leaves the tree untouched
    auto checkpointId = repository->commitMessage(
        CommitMessage::checkpoint({"dXBkYXRlMQ==", "dXBkYXRlMg=="}).toString());
    CPPUNIT_ASSERT(!checkpointId.empty());
    auto commit = repository->getCommit(checkpointId);
    CPPUNIT_ASSERT(commit.has_value());
    CPPUNIT_ASSERT_EQUAL(std::string(CommitType::CHECKPOINT), commit->commitMsg.type);
    CPPUNIT_ASSERT_EQUAL("dXBkYXRlMQ==\ndXBkYXRlMg=="s, commit->commitMsg.body);

    // Attachments are content-addressed blobs under attachments/
    std::vector<uint8_t> data {0x01, 0x02, 0x03, 0x04};
    auto attachmentId = repository->addAttachment(data);
    CPPUNIT_ASSERT(!attachmentId.empty());
    CPPUNIT_ASSERT(repository->attachment(attachmentId) == data);
    CPPUNIT_ASSERT(repository->attachmentIds() == std::vector<std::string> {attachmentId});

    // Adding the same content again converges to the same entry
    CPPUNIT_ASSERT_EQUAL(attachmentId, repository->addAttachment(data));
    CPPUNIT_ASSERT(repository->attachmentIds().size() == 1);

    // Everything above passes validation
    CPPUNIT_ASSERT(repository->validCommits(repository->log()));
}

// A checkpoint only means something in a document repository: in a
// conversation it must be rejected.
void
ConversationRepositoryTest::testCheckpointInConversationRejected()
{
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool isInvalid = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::OnConversationError>(
        [&](const std::string&, const std::string&, int code, const std::string&) {
            if (code == EVALIDFETCH)
                isInvalid = true;
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto repository = ConversationRepository::createConversation(aliceAccount);
    CPPUNIT_ASSERT(repository != nullptr);

    auto repoPath = fileutils::get_data_dir() / aliceAccount->getAccountID() / "conversations" / repository->id();
    git_repository* repo;
    CPPUNIT_ASSERT(git_repository_open(&repo, repoPath.c_str()) == 0);
    auto checkpointId = addCommit(repo, aliceAccount, "main", CommitMessage::checkpoint({"dXBkYXRlMQ=="}).toString());
    git_repository_free(repo);
    CPPUNIT_ASSERT(!checkpointId.empty());

    CPPUNIT_ASSERT(!repository->validCommits(repository->log()));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return isInvalid; }));
}

// A checkpoint may only touch attachments/ (and the author's device
// certificate): any other file makes it invalid.
void
ConversationRepositoryTest::testCheckpointWithInvalidFileRejected()
{
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool isInvalid = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::OnConversationError>(
        [&](const std::string&, const std::string&, int code, const std::string&) {
            if (code == EVALIDFETCH)
                isInvalid = true;
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto repository = ConversationRepository::createDocument(aliceAccount, "some-conversation-id", "text/html");
    CPPUNIT_ASSERT(repository != nullptr);
    auto repoPath = fileutils::get_data_dir() / aliceAccount->getAccountID() / "conversations" / repository->id();

    git_repository* repo;
    CPPUNIT_ASSERT(git_repository_open(&repo, repoPath.c_str()) == 0);

    std::ofstream(repoPath / "invalidFile.txt").close();
    addAll(repo);
    auto commitId = addCommit(repo, aliceAccount, "main", CommitMessage::checkpoint({"dXBkYXRlMQ=="}).toString());
    git_repository_free(repo);
    CPPUNIT_ASSERT(!commitId.empty());

    CPPUNIT_ASSERT(!repository->validCommits(repository->log()));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return isInvalid; }));
}

// An attachment's name must be the git oid of its own content, so that two
// entries sharing a name necessarily hold the same bytes.
void
ConversationRepositoryTest::testNonContentAddressedAttachmentRejected()
{
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool isInvalid = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::OnConversationError>(
        [&](const std::string&, const std::string&, int code, const std::string&) {
            if (code == EVALIDFETCH)
                isInvalid = true;
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto repository = ConversationRepository::createDocument(aliceAccount, "some-conversation-id", "text/html");
    CPPUNIT_ASSERT(repository != nullptr);
    auto repoPath = fileutils::get_data_dir() / aliceAccount->getAccountID() / "conversations" / repository->id();

    git_repository* repo;
    CPPUNIT_ASSERT(git_repository_open(&repo, repoPath.c_str()) == 0);

    std::filesystem::create_directories(repoPath / "attachments");
    std::ofstream file(repoPath / "attachments" / "0000000000000000000000000000000000000000");
    file << "not the content this name promises";
    file.close();
    addAll(repo);
    auto commitId = addCommit(repo, aliceAccount, "main", CommitMessage::checkpoint({}).toString());
    git_repository_free(repo);
    CPPUNIT_ASSERT(!commitId.empty());

    CPPUNIT_ASSERT(!repository->validCommits(repository->log()));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return isInvalid; }));
}

// A checkpoint may touch the author's own device certificate, but only to
// add or update it: one that deletes it must be rejected, not crash the
// validator on the missing blob.
void
ConversationRepositoryTest::testCheckpointDeletingDeviceCertRejected()
{
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool isInvalid = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::OnConversationError>(
        [&](const std::string&, const std::string&, int code, const std::string&) {
            if (code == EVALIDFETCH)
                isInvalid = true;
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceDeviceId = DeviceId(std::string(aliceAccount->currentDeviceId()));
    auto repository = ConversationRepository::createDocument(aliceAccount, "some-conversation-id", "text/html");
    CPPUNIT_ASSERT(repository != nullptr);
    auto repoPath = fileutils::get_data_dir() / aliceAccount->getAccountID() / "conversations" / repository->id();

    git_repository* repo;
    CPPUNIT_ASSERT(git_repository_open(&repo, repoPath.c_str()) == 0);

    // Stage the deletion of the author's own device certificate.
    auto deviceFile = fmt::format("devices/{}.crt", aliceDeviceId.toString());
    std::filesystem::remove(repoPath / deviceFile);
    git_index* index_ptr = nullptr;
    CPPUNIT_ASSERT(git_repository_index(&index_ptr, repo) == 0);
    GitIndex index {index_ptr};
    CPPUNIT_ASSERT(git_index_remove_bypath(index.get(), deviceFile.c_str()) == 0);
    CPPUNIT_ASSERT(git_index_write(index.get()) == 0);

    auto commitId = addCommit(repo, aliceAccount, "main", CommitMessage::checkpoint({"dXBkYXRlMQ=="}).toString());
    git_repository_free(repo);
    CPPUNIT_ASSERT(!commitId.empty());

    CPPUNIT_ASSERT(!repository->validCommits(repository->log()));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return isInvalid; }));
}

// Attachments only exist in document repositories: in a conversation,
// addAttachment() must refuse rather than write a checkpoint commit its own
// validator then rejects.
void
ConversationRepositoryTest::testAttachmentInConversationRejected()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto repository = ConversationRepository::createConversation(aliceAccount);
    CPPUNIT_ASSERT(repository != nullptr);

    auto headBefore = repository->getHead();
    std::vector<uint8_t> data {0x01, 0x02, 0x03, 0x04};
    CPPUNIT_ASSERT(repository->addAttachment(data).empty());
    CPPUNIT_ASSERT_EQUAL(headBefore, repository->getHead());
    CPPUNIT_ASSERT(repository->validCommits(repository->log()));
}

// The mirror image of testCheckpointInConversationRejected: free-form message
// commits (text, call history, data transfers…) only belong to conversations,
// so a document repository must reject them.
void
ConversationRepositoryTest::testMessageInDocumentRejected()
{
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    bool isInvalid = false;
    confHandlers.insert(libjami::exportable_callback<libjami::ConversationSignal::OnConversationError>(
        [&](const std::string&, const std::string&, int code, const std::string&) {
            if (code == EVALIDFETCH)
                isInvalid = true;
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto repository = ConversationRepository::createDocument(aliceAccount, "some-conversation-id", "text/html");
    CPPUNIT_ASSERT(repository != nullptr);

    auto repoPath = fileutils::get_data_dir() / aliceAccount->getAccountID() / "conversations" / repository->id();
    git_repository* repo;
    CPPUNIT_ASSERT(git_repository_open(&repo, repoPath.c_str()) == 0);
    auto messageId = addCommit(repo, aliceAccount, "main", R"({"body":"hello","type":"text/plain"})");
    git_repository_free(repo);
    CPPUNIT_ASSERT(!messageId.empty());

    CPPUNIT_ASSERT(!repository->validCommits(repository->log()));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return isInvalid; }));
}

// Every fetch stores what it receives in a pack of its own. Once there are
// too many, they are merged, and the history reads and validates as before.
void
ConversationRepositoryTest::testRepackKeepsHistory()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto repository = ConversationRepository::createConversation(aliceAccount);
    CPPUNIT_ASSERT(repository != nullptr);
    auto repoPath = fileutils::get_data_dir() / aliceAccount->getAccountID() / "conversations" / repository->id();
    auto objectsPath = repoPath / ".git" / "objects";

    // Move a commit and its tree from the loose objects to a pack, as a fetch would store them
    auto storeInPack = [&](const std::string& commitId) {
        git_repository* repo = nullptr;
        CPPUNIT_ASSERT_EQUAL(0, git_repository_open(&repo, repoPath.string().c_str()));
        git_packbuilder* pb = nullptr;
        CPPUNIT_ASSERT_EQUAL(0, git_packbuilder_new(&pb, repo));
        git_oid oid;
        CPPUNIT_ASSERT_EQUAL(0, git_oid_fromstr(&oid, commitId.c_str()));
        CPPUNIT_ASSERT_EQUAL(0, git_packbuilder_insert_commit(pb, &oid));
        CPPUNIT_ASSERT_EQUAL(0, git_packbuilder_write(pb, (objectsPath / "pack").string().c_str(), 0, nullptr, nullptr));
        git_packbuilder_free(pb);
        git_repository_free(repo);
        for (const auto& entry : std::filesystem::directory_iterator(objectsPath))
            if (entry.path().filename().string().size() == 2)
                std::filesystem::remove_all(entry.path());
    };
    auto countPacks = [&] {
        std::size_t count = 0;
        for (const auto& entry : std::filesystem::directory_iterator(objectsPath / "pack"))
            count += entry.path().extension() == ".pack";
        return count;
    };
    storeInPack(repository->getHead());
    for (auto i = 0; i < 60; ++i)
        storeInPack(repository->commitMessage(fmt::format(R"({{"body":"message {}","type":"text/plain"}})", i)));
    CPPUNIT_ASSERT_EQUAL(std::size_t(61), countPacks());
    auto before = repository->log();
    CPPUNIT_ASSERT_EQUAL(std::size_t(61), before.size());

    // Only fetches add packs: without one, there is nothing to merge
    repository->repack();
    CPPUNIT_ASSERT_EQUAL(std::size_t(61), countPacks());

    // Fetch a commit from a copy of the conversation, as from another device
    auto peerPath = fileutils::get_data_dir() / "repackpeer";
    git_repository* peer = nullptr;
    CPPUNIT_ASSERT_EQUAL(0,
                         git_clone(&peer,
                                   repoPath.string().c_str(),
                                   (peerPath / "conversations" / repository->id()).string().c_str(),
                                   nullptr));
    git_object* head = nullptr;
    CPPUNIT_ASSERT_EQUAL(0, git_revparse_single(&head, peer, "HEAD"));
    git_tree* tree = nullptr;
    CPPUNIT_ASSERT_EQUAL(0, git_commit_tree(&tree, reinterpret_cast<git_commit*>(head)));
    git_signature* sig = nullptr;
    CPPUNIT_ASSERT_EQUAL(0, git_signature_now(&sig, "peer", "peer@jami.net"));
    git_oid peerCommit;
    CPPUNIT_ASSERT_EQUAL(0,
                         git_commit_create_v(&peerCommit,
                                             peer,
                                             "HEAD",
                                             sig,
                                             sig,
                                             nullptr,
                                             "from the peer",
                                             tree,
                                             1,
                                             reinterpret_cast<const git_commit*>(head)));
    git_signature_free(sig);
    git_tree_free(tree);
    git_object_free(head);
    git_repository_free(peer);
    const bool savedFlag = ConversationRepository::FETCH_FROM_LOCAL_REPOS;
    ConversationRepository::FETCH_FROM_LOCAL_REPOS = true;
    auto fetched = repository->fetch("repackpeer");
    ConversationRepository::FETCH_FROM_LOCAL_REPOS = savedFlag;
    std::filesystem::remove_all(peerPath);
    CPPUNIT_ASSERT(fetched);
    CPPUNIT_ASSERT_EQUAL(std::size_t(62), countPacks());

    repository->repack();

    CPPUNIT_ASSERT_EQUAL(std::size_t(1), countPacks());
    auto after = repository->log();
    CPPUNIT_ASSERT_EQUAL(before.size(), after.size());
    for (std::size_t i = 0; i < before.size(); ++i)
        CPPUNIT_ASSERT_EQUAL(before[i].id, after[i].id);
    CPPUNIT_ASSERT(repository->validCommits(after));
    // The fetched commit, which nothing merged, is kept as well
    git_repository* repo = nullptr;
    CPPUNIT_ASSERT_EQUAL(0, git_repository_open(&repo, repoPath.string().c_str()));
    git_odb* odb = nullptr;
    CPPUNIT_ASSERT_EQUAL(0, git_repository_odb(&odb, repo));
    CPPUNIT_ASSERT(git_odb_exists(odb, &peerCommit));
    git_odb_free(odb);
    git_repository_free(repo);
}

void
ConversationRepositoryTest::testModePermissionsRejected()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    struct Scenario
    {
        ConversationMode mode;
        std::string peer;
        std::string message;
    };
    const std::array<Scenario, 3> cases {{
        {ConversationMode::ONE_TO_ONE, bobAccount->getUsername(), R"({"type":"application/update-profile"})"},
        {ConversationMode::ONE_TO_ONE, std::string(40, 'a'), R"({"type":"application/call-history+json","confId":"1"})"},
        {ConversationMode::INVITES_ONLY, "", R"({"type":"application/call-history+json","to":"peer"})"},
    }};
    for (const auto& [mode, peer, message] : cases) {
        auto repository = ConversationRepository::createConversation(aliceAccount, mode, peer);
        CPPUNIT_ASSERT(repository != nullptr);
        auto repoPath = fileutils::get_data_dir() / aliceAccount->getAccountID() / "conversations" / repository->id();
        git_repository* repo;
        CPPUNIT_ASSERT(git_repository_open(&repo, repoPath.c_str()) == 0);
        auto commitId = addCommit(repo, aliceAccount, "main", message);
        git_repository_free(repo);
        CPPUNIT_ASSERT(!commitId.empty());
        CPPUNIT_ASSERT(!repository->validCommits(repository->log()));
    }
}

void
ConversationRepositoryTest::testLocalPermissionDenied()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto repository = ConversationRepository::createDocument(aliceAccount, "local-permission-test", "text/html");
    CPPUNIT_ASSERT(repository != nullptr);

    auto head = repository->getHead();
    CPPUNIT_ASSERT(repository->commitMessage(CommitMessage::text("hello").toString()).empty());
    CPPUNIT_ASSERT(repository->commitMessage(R"({"type":"application/call-history+json"})").empty());
    CPPUNIT_ASSERT_EQUAL(head, repository->getHead());

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto conversation = ConversationRepository::createConversation(aliceAccount,
                                                                   ConversationMode::ONE_TO_ONE,
                                                                   bobAccount->getUsername());
    CPPUNIT_ASSERT(conversation != nullptr);
    head = conversation->getHead();
    CPPUNIT_ASSERT(conversation->commitMessage(CommitMessage::checkpoint({}).toString()).empty());
    CPPUNIT_ASSERT(conversation->commitMessage(R"({"type":"application/update-profile"})").empty());
    CPPUNIT_ASSERT(conversation->commitMessage(R"({"type":"application/call-history+json","confId":"1"})").empty());
    CPPUNIT_ASSERT(
        conversation->commitMessage(CommitMessage::member(CommitAction::JOIN, bobAccount->getUsername()).toString())
            .empty());
    CPPUNIT_ASSERT(
        conversation->commitMessage(CommitMessage::member(CommitAction::REMOVE, bobAccount->getUsername()).toString())
            .empty());
    CPPUNIT_ASSERT_EQUAL(head, conversation->getHead());
}

void
ConversationRepositoryTest::testRemoveOtherMemberRejected()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto repository = ConversationRepository::createConversation(aliceAccount);
    CPPUNIT_ASSERT(repository != nullptr);

    auto repoPath = fileutils::get_data_dir() / aliceAccount->getAccountID() / "conversations" / repository->id();
    git_repository* repo;
    CPPUNIT_ASSERT(git_repository_open(&repo, repoPath.c_str()) == 0);
    auto commitId = addCommit(repo,
                              aliceAccount,
                              "main",
                              CommitMessage::member(CommitAction::REMOVE, std::string(40, 'a')).toString());
    git_repository_free(repo);
    CPPUNIT_ASSERT(!commitId.empty());

    CPPUNIT_ASSERT(!repository->validCommits(repository->log()));
}

void
ConversationRepositoryTest::testUnknownCommitTypeInDocumentRejected()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto repository = ConversationRepository::createDocument(aliceAccount, "unknown-type-test", "text/html");
    CPPUNIT_ASSERT(repository != nullptr);
    const std::string message = R"({"type":"application/x-unknown","body":"hello"})";

    auto head = repository->getHead();
    CPPUNIT_ASSERT(repository->commitMessage(message).empty());
    CPPUNIT_ASSERT_EQUAL(head, repository->getHead());

    auto repoPath = fileutils::get_data_dir() / aliceAccount->getAccountID() / "conversations" / repository->id();
    git_repository* repo;
    CPPUNIT_ASSERT(git_repository_open(&repo, repoPath.c_str()) == 0);
    auto commitId = addCommit(repo, aliceAccount, "main", message);
    git_repository_free(repo);
    CPPUNIT_ASSERT(!commitId.empty());

    CPPUNIT_ASSERT(!repository->validCommits(repository->log()));
}

void
ConversationRepositoryTest::testJoinLeavePermissions()
{
    for (auto mode : {ConversationMode::ONE_TO_ONE, ConversationMode::INVITES_ONLY, ConversationMode::DOCUMENT}) {
        CPPUNIT_ASSERT(hasPermission(mode, MemberRole::INVITED, Permission::Join));
        CPPUNIT_ASSERT(!hasPermission(mode, MemberRole::INVITED, Permission::Leave));
        CPPUNIT_ASSERT(!hasPermission(mode, MemberRole::INVITED, Permission::Text));
        for (auto role : {MemberRole::ADMIN, MemberRole::MEMBER}) {
            CPPUNIT_ASSERT(!hasPermission(mode, role, Permission::Join));
            CPPUNIT_ASSERT(hasPermission(mode, role, Permission::Leave));
        }
        for (auto role : {MemberRole::BANNED, MemberRole::LEFT}) {
            CPPUNIT_ASSERT(!hasPermission(mode, role, Permission::Join));
            CPPUNIT_ASSERT(!hasPermission(mode, role, Permission::Leave));
        }
    }

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto repository = ConversationRepository::createConversation(aliceAccount);
    CPPUNIT_ASSERT(repository != nullptr);

    // Alice is already an admin, so she was not invited and is unable to join
    auto head = repository->getHead();
    CPPUNIT_ASSERT(
        repository->commitMessage(CommitMessage::member(CommitAction::JOIN, aliceAccount->getUsername()).toString())
            .empty());
    CPPUNIT_ASSERT_EQUAL(head, repository->getHead());

    CPPUNIT_ASSERT(!repository->leave().empty());
}

void
ConversationRepositoryTest::testMemberPermissions()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto contains = [](const std::vector<std::string>& names, std::string_view name) {
        return std::find(names.begin(), names.end(), name) != names.end();
    };

    for (const auto& [permission, name] : PERMISSION_NAME_TABLE) {
        CPPUNIT_ASSERT(!name.empty());
        CPPUNIT_ASSERT(permissionName(permission) == name);
    }

    auto group = ConversationRepository::createConversation(aliceAccount, ConversationMode::INVITES_ONLY);
    CPPUNIT_ASSERT(group != nullptr);
    auto adminPermissions = group->permissions(aliceUri);
    CPPUNIT_ASSERT(adminPermissions == permissionNames(ConversationMode::INVITES_ONLY, MemberRole::ADMIN));
    CPPUNIT_ASSERT(contains(adminPermissions, "updateProfile"));
    CPPUNIT_ASSERT(contains(adminPermissions, "hostConference"));
    CPPUNIT_ASSERT(contains(adminPermissions, "leave"));
    CPPUNIT_ASSERT(!contains(adminPermissions, "call"));
    CPPUNIT_ASSERT(group->permissions(bobUri).empty());

    auto memberPermissions = permissionNames(ConversationMode::INVITES_ONLY, MemberRole::MEMBER);
    CPPUNIT_ASSERT(contains(memberPermissions, "text"));
    CPPUNIT_ASSERT(contains(memberPermissions, "leave"));
    CPPUNIT_ASSERT(!contains(memberPermissions, "updateProfile"));
    CPPUNIT_ASSERT(!contains(memberPermissions, "banUnbanMember"));
    CPPUNIT_ASSERT(permissionNames(ConversationMode::INVITES_ONLY, MemberRole::INVITED)
                   == std::vector<std::string> {"join"});
    CPPUNIT_ASSERT(permissionNames(ConversationMode::INVITES_ONLY, MemberRole::BANNED).empty());

    auto oneToOne = ConversationRepository::createConversation(aliceAccount, ConversationMode::ONE_TO_ONE, bobUri);
    CPPUNIT_ASSERT(oneToOne != nullptr);
    auto oneToOnePermissions = oneToOne->permissions(aliceUri);
    CPPUNIT_ASSERT(contains(oneToOnePermissions, "call"));
    CPPUNIT_ASSERT(!contains(oneToOnePermissions, "hostConference"));
    CPPUNIT_ASSERT(!contains(oneToOnePermissions, "updateProfile"));
    // Bob is only invited until he joins
    CPPUNIT_ASSERT(oneToOne->permissions(bobUri) == std::vector<std::string> {"join"});

    const auto feedMode = ConversationMode::FEED;
    auto feed = ConversationRepository::createConversation(aliceAccount, feedMode);
    CPPUNIT_ASSERT(feed != nullptr);
    CPPUNIT_ASSERT(feed->mode() == feedMode);
    const auto feedAdminPermissions = feed->permissions(aliceUri);
    CPPUNIT_ASSERT(contains(feedAdminPermissions, "text"));
    CPPUNIT_ASSERT(contains(feedAdminPermissions, "textReply"));
    CPPUNIT_ASSERT(contains(feedAdminPermissions, "file"));
    CPPUNIT_ASSERT(contains(feedAdminPermissions, "fileReply"));
    CPPUNIT_ASSERT(contains(feedAdminPermissions, "updateProfile"));

    const auto feedMemberPermissions = permissionNames(feedMode, MemberRole::MEMBER);
    CPPUNIT_ASSERT(!contains(feedMemberPermissions, "text"));
    CPPUNIT_ASSERT(!contains(feedMemberPermissions, "file"));
    CPPUNIT_ASSERT(contains(feedMemberPermissions, "textReply"));
    CPPUNIT_ASSERT(contains(feedMemberPermissions, "fileReply"));
    CPPUNIT_ASSERT(contains(feedMemberPermissions, "reaction"));
}

void
ConversationRepositoryTest::testFeedFollowerCanRejoin()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto feed = ConversationRepository::createConversation(aliceAccount, ConversationMode::FEED);
    const auto bobUri = bobAccount->getUsername();
    CPPUNIT_ASSERT(!feed->addMember(bobUri).empty());
    const auto post = feed->commitMessage(CommitMessage::text("Before subscribing").toString());
    CPPUNIT_ASSERT(!post.empty());

    const auto alicePath = fileutils::get_data_dir() / aliceId / "conversations" / feed->id();
    const auto bobPath = fileutils::get_data_dir() / bobId / "conversations" / feed->id();
    std::filesystem::create_directories(bobPath.parent_path());
    std::filesystem::copy(alicePath, bobPath, std::filesystem::copy_options::recursive);
    ConversationRepository follower(bobAccount, feed->id());
    CPPUNIT_ASSERT(!follower.join().empty());
    CPPUNIT_ASSERT(!follower.leave().empty());
    CPPUNIT_ASSERT(follower.validCommits(follower.log()));
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(bobPath / "invited" / bobUri));
    CPPUNIT_ASSERT(!std::filesystem::exists(bobPath / "members" / (bobUri + ".crt")));
    CPPUNIT_ASSERT(follower.permissions(bobUri) == std::vector<std::string> {"join"});

    CPPUNIT_ASSERT(!follower.join().empty());
    CPPUNIT_ASSERT(follower.validCommits(follower.log()));
    CPPUNIT_ASSERT(follower.getCommit(post).has_value());
    CPPUNIT_ASSERT(!std::filesystem::exists(bobPath / "invited" / bobUri));
    CPPUNIT_ASSERT(std::filesystem::is_regular_file(bobPath / "members" / (bobUri + ".crt")));
    CPPUNIT_ASSERT(!hasPermission(ConversationMode::FEED, MemberRole::ADMIN, Permission::Leave));
}

void
ConversationRepositoryTest::testFeedJoinStillRequiresAuthorization()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    const auto bobUri = bobAccount->getUsername();
    for (bool banned : {false, true}) {
        auto feed = ConversationRepository::createConversation(aliceAccount, ConversationMode::FEED);
        CPPUNIT_ASSERT(feed != nullptr);
        if (banned) {
            CPPUNIT_ASSERT(!feed->addMember(bobUri).empty());
            CPPUNIT_ASSERT(!feed->voteKick(bobUri, "invited").empty());
            CPPUNIT_ASSERT(!feed->resolveVote(bobUri, "invited", "ban").empty());
        }
        const auto alicePath = fileutils::get_data_dir() / aliceId / "conversations" / feed->id();
        const auto bobPath = fileutils::get_data_dir() / bobId / "conversations" / feed->id();
        std::filesystem::create_directories(bobPath.parent_path());
        std::filesystem::copy(alicePath, bobPath, std::filesystem::copy_options::recursive);
        ConversationRepository unauthorized(bobAccount, feed->id());
        CPPUNIT_ASSERT(unauthorized.join().empty());
        CPPUNIT_ASSERT(feed->validCommits(feed->log()));
        Conversation owner(aliceAccount, feed->id());
        CPPUNIT_ASSERT(!owner.isPeerAuthorized(bobUri, std::string(bobAccount->currentDeviceId()), true));
        unauthorized.erase();
        feed->erase();
    }
}

void
ConversationRepositoryTest::testFeedMemberActions()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceRepository = ConversationRepository::createConversation(aliceAccount, ConversationMode::FEED);
    CPPUNIT_ASSERT(aliceRepository != nullptr);
    CPPUNIT_ASSERT(!aliceRepository->addMember(bobAccount->getUsername()).empty());
    CommitMessage unknownMessage;
    unknownMessage.type = "application/x-unknown";
    CPPUNIT_ASSERT(!aliceRepository->commitMessage(unknownMessage.toString()).empty());

    const auto postId = aliceRepository->commitMessage(CommitMessage::text("Admin post").toString());
    const auto fileId = aliceRepository->commitMessage(
        CommitMessage::fileSent("admin.txt", std::string(128, 'a'), 1, 10).toString());
    CPPUNIT_ASSERT(!postId.empty());
    CPPUNIT_ASSERT(!fileId.empty());
    CPPUNIT_ASSERT(aliceRepository->validCommits(aliceRepository->log()));

    const auto alicePath = fileutils::get_data_dir() / aliceId / "conversations" / aliceRepository->id();
    const auto bobPath = fileutils::get_data_dir() / bobId / "conversations" / aliceRepository->id();
    std::filesystem::create_directories(bobPath.parent_path());
    std::filesystem::copy(alicePath, bobPath, std::filesystem::copy_options::recursive);
    ConversationRepository bobRepository(bobAccount, aliceRepository->id());
    const auto invitedHead = bobRepository.getHead();
    CPPUNIT_ASSERT(bobRepository.commitMessage(unknownMessage.toString()).empty());
    CPPUNIT_ASSERT_EQUAL(invitedHead, bobRepository.getHead());
    CPPUNIT_ASSERT(!bobRepository.join().empty());
    CPPUNIT_ASSERT(bobRepository.validCommits(bobRepository.log()));

    const auto replyId = bobRepository.commitMessage(CommitMessage::text("Member reply", postId).toString());
    const auto fileReplyId = bobRepository.commitMessage(
        CommitMessage::fileSent("reply.txt", std::string(128, 'b'), 2, 10, postId).toString());
    CPPUNIT_ASSERT(!replyId.empty());
    CPPUNIT_ASSERT(!fileReplyId.empty());
    CPPUNIT_ASSERT(!bobRepository.commitMessage(CommitMessage::edit("Edited reply", replyId).toString()).empty());
    CPPUNIT_ASSERT(!bobRepository.commitMessage(CommitMessage::edit("", replyId).toString()).empty());
    CPPUNIT_ASSERT(!bobRepository.commitMessage(CommitMessage::fileDeleted(fileReplyId).toString()).empty());
    CPPUNIT_ASSERT(bobRepository.validCommits(bobRepository.log()));

    git_repository* repoPtr = nullptr;
    CPPUNIT_ASSERT_EQUAL(0, git_repository_open(&repoPtr, bobPath.c_str()));
    GitRepository repo {repoPtr};
    const auto head = bobRepository.getHead();
    const std::array<CommitMessage, 6> forbidden {{
        unknownMessage,
        CommitMessage::text("Member post"),
        CommitMessage::fileSent("member.txt", std::string(128, 'c'), 3, 10),
        CommitMessage::edit("Changed admin post", postId),
        CommitMessage::edit("", postId),
        CommitMessage::fileDeleted(fileId),
    }};
    for (const auto& message : forbidden) {
        CPPUNIT_ASSERT(bobRepository.commitMessage(message.toString()).empty());
        CPPUNIT_ASSERT_EQUAL(head, bobRepository.getHead());

        const auto commitId = addCommit(repo.get(), bobAccount, "rejected", message.toString());
        CPPUNIT_ASSERT(!commitId.empty());
        const auto commit = bobRepository.getCommit(commitId);
        CPPUNIT_ASSERT(commit.has_value());
        CPPUNIT_ASSERT(!bobRepository.validCommits({*commit}));
        CPPUNIT_ASSERT_EQUAL(head, bobRepository.getHead());
    }
}

bool
ConversationRepositoryTest::validBanResolution(std::size_t voterCount, bool dropSecondAdminFromWorkdir)
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    const std::string bannedUri(40, 'c');

    // Call once per test: a second conversation created in the same second gets the same id
    auto repository = ConversationRepository::createConversation(aliceAccount);
    CPPUNIT_ASSERT(repository != nullptr);
    auto repoPath = fileutils::get_data_dir() / aliceAccount->getAccountID() / "conversations" / repository->id();
    git_repository* repo = nullptr;
    CPPUNIT_ASSERT(git_repository_open(&repo, repoPath.c_str()) == 0);

    // Raw commits that are never validated: Bob becomes a second admin, and bannedUri a member.
    // A valid history is unable to hold two admins yet.
    auto aliceCertificate = repoPath / "admins" / (aliceUri + ".crt");
    std::filesystem::copy_file(aliceCertificate, repoPath / "admins" / (bobUri + ".crt"));
    std::filesystem::create_directories(repoPath / "members");
    std::filesystem::copy_file(aliceCertificate, repoPath / "members" / (bannedUri + ".crt"));
    addAll(repo);
    CPPUNIT_ASSERT(!addCommit(repo, aliceAccount, "main", R"({"type":"text/plain","body":"setup"})").empty());

    // The first voterCount admins vote to ban bannedUri
    std::vector<std::string> voters {aliceUri, bobUri};
    voters.resize(voterCount);
    auto voteDirectory = repoPath / "votes" / "ban" / "members" / bannedUri;
    std::filesystem::create_directories(voteDirectory);
    for (const auto& voter : voters)
        std::ofstream(voteDirectory / voter).close();
    addAll(repo);
    CPPUNIT_ASSERT(!addCommit(repo, aliceAccount, "main", CommitMessage::vote(bannedUri).toString()).empty());

    // Resolution as resolveVote writes it: votes removed, member certificate moved to banned/
    git_index* index_ptr = nullptr;
    CPPUNIT_ASSERT(git_repository_index(&index_ptr, repo) == 0);
    GitIndex index {index_ptr};
    for (const auto& voter : voters) {
        std::filesystem::remove(voteDirectory / voter);
        auto votePath = fmt::format("votes/ban/members/{}/{}", bannedUri, voter);
        CPPUNIT_ASSERT(git_index_remove_bypath(index.get(), votePath.c_str()) == 0);
    }
    std::filesystem::create_directories(repoPath / "banned" / "members");
    std::filesystem::rename(repoPath / "members" / (bannedUri + ".crt"),
                            repoPath / "banned" / "members" / (bannedUri + ".crt"));
    auto memberPath = fmt::format("members/{}.crt", bannedUri);
    CPPUNIT_ASSERT(git_index_remove_bypath(index.get(), memberPath.c_str()) == 0);
    CPPUNIT_ASSERT(git_index_write(index.get()) == 0);
    index.reset();
    addAll(repo);
    auto resolutionId = addCommit(repo,
                                  aliceAccount,
                                  "main",
                                  CommitMessage::member(CommitAction::BAN, bannedUri).toString());
    git_repository_free(repo);
    CPPUNIT_ASSERT(!resolutionId.empty());

    if (dropSecondAdminFromWorkdir)
        std::filesystem::remove(repoPath / "admins" / (bobUri + ".crt"));

    auto resolution = repository->getCommit(resolutionId);
    CPPUNIT_ASSERT(resolution.has_value());
    return repository->validCommits({*resolution});
}

void
ConversationRepositoryTest::testVoteResolutionHalfRejected()
{
    // One vote of two admins is exactly half, which is no majority
    CPPUNIT_ASSERT(!validBanResolution(1, false));
}

void
ConversationRepositoryTest::testVoteResolutionMajorityAccepted()
{
    CPPUNIT_ASSERT(validBanResolution(2, false));
}

void
ConversationRepositoryTest::testVoteResolutionCountsParentTree()
{
    // The working directory lists Alice alone as admin, while the parent commit lists Alice and
    // Bob. One vote is then all admins of the working directory, but only half of the parent's
    CPPUNIT_ASSERT(!validBanResolution(1, true));
}

ConversationRepositoryTest::HandBuiltSwarm
ConversationRepositoryTest::handBuiltSwarm(const std::vector<std::string>& extraAdmins,
                                           const std::vector<std::string>& members,
                                           const std::string& approvalThreshold)
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    HandBuiltSwarm swarm;
    // Call once per test: a second conversation created in the same second gets the same id
    swarm.repository = ConversationRepository::createConversation(aliceAccount);
    CPPUNIT_ASSERT(swarm.repository != nullptr);
    swarm.path = fileutils::get_data_dir() / aliceAccount->getAccountID() / "conversations" / swarm.repository->id();
    git_repository* repoPtr = nullptr;
    CPPUNIT_ASSERT_EQUAL(0, git_repository_open(&repoPtr, swarm.path.c_str()));
    swarm.repo.reset(repoPtr);

    // Raw commit that is never validated, because a valid history is unable to hold two admins
    // before role changes exist. Every certificate is a copy of Alice's, and only the URI in the
    // file name tells them apart, which is all that the validation of votes looks at.
    auto certificate = fileContent(swarm, fmt::format("admins/{}.crt", aliceAccount->getUsername()));
    for (const auto& uri : extraAdmins)
        writeFile(swarm, fmt::format("admins/{}.crt", uri), certificate);
    for (const auto& uri : members)
        writeFile(swarm, fmt::format("members/{}.crt", uri), certificate);
    if (!approvalThreshold.empty())
        writeFile(swarm, std::string(ApprovalThreshold::FILE_PATH), approvalThreshold);
    commitRaw(swarm, CommitMessage::text("setup"));
    return swarm;
}

std::string
ConversationRepositoryTest::fileContent(const HandBuiltSwarm& swarm, const std::string& path)
{
    std::ifstream file(swarm.path / path, std::ios::binary);
    CPPUNIT_ASSERT(file.is_open());
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

void
ConversationRepositoryTest::writeFile(HandBuiltSwarm& swarm, const std::string& path, const std::string& content)
{
    auto fullPath = swarm.path / path;
    std::filesystem::create_directories(fullPath.parent_path());
    std::ofstream file(fullPath, std::ios::binary | std::ios::trunc);
    file << content;
    file.close();

    git_index* indexPtr = nullptr;
    CPPUNIT_ASSERT_EQUAL(0, git_repository_index(&indexPtr, swarm.repo.get()));
    GitIndex index {indexPtr};
    CPPUNIT_ASSERT_EQUAL(0, git_index_add_bypath(index.get(), path.c_str()));
    CPPUNIT_ASSERT_EQUAL(0, git_index_write(index.get()));
}

void
ConversationRepositoryTest::removeFile(HandBuiltSwarm& swarm, const std::string& path)
{
    CPPUNIT_ASSERT(std::filesystem::remove(swarm.path / path));

    git_index* indexPtr = nullptr;
    CPPUNIT_ASSERT_EQUAL(0, git_repository_index(&indexPtr, swarm.repo.get()));
    GitIndex index {indexPtr};
    CPPUNIT_ASSERT_EQUAL(0, git_index_remove_bypath(index.get(), path.c_str()));
    CPPUNIT_ASSERT_EQUAL(0, git_index_write(index.get()));
}

void
ConversationRepositoryTest::moveFile(HandBuiltSwarm& swarm, const std::string& from, const std::string& to)
{
    auto content = fileContent(swarm, from);
    removeFile(swarm, from);
    writeFile(swarm, to, content);
}

std::string
ConversationRepositoryTest::commitRaw(HandBuiltSwarm& swarm, const CommitMessage& message)
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto commitId = addCommit(swarm.repo.get(), aliceAccount, "main", message.toString());
    CPPUNIT_ASSERT(!commitId.empty());
    return commitId;
}

bool
ConversationRepositoryTest::commitValidates(HandBuiltSwarm& swarm, const CommitMessage& message, bool keepOnMain)
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto commitId = addCommit(swarm.repo.get(), aliceAccount, keepOnMain ? "main" : "rejected", message.toString());
    CPPUNIT_ASSERT(!commitId.empty());
    if (!keepOnMain) {
        // The commit stays on a side branch, and the index and the working directory return to main
        git_object* mainPtr = nullptr;
        CPPUNIT_ASSERT_EQUAL(0, git_revparse_single(&mainPtr, swarm.repo.get(), "refs/heads/main"));
        GitObject mainCommit {mainPtr};
        CPPUNIT_ASSERT_EQUAL(0, git_reset(swarm.repo.get(), mainCommit.get(), GIT_RESET_HARD, nullptr));
    }
    auto commit = swarm.repository->getCommit(commitId);
    CPPUNIT_ASSERT(commit.has_value());
    return swarm.repository->validCommits({*commit});
}

MemberRole
ConversationRepositoryTest::roleOf(const HandBuiltSwarm& swarm, const std::string& uri)
{
    swarm.repository->refreshMembers();
    for (const auto& member : swarm.repository->members()) {
        if (member.uri == uri)
            return member.role;
    }
    CPPUNIT_FAIL("No member " + uri);
    return MemberRole::LEFT;
}

void
ConversationRepositoryTest::testApprovalThresholdParse()
{
    for (std::string_view token : {"percent/1", "percent/50", "percent/100"}) {
        auto threshold = ApprovalThreshold::parse(token);
        CPPUNIT_ASSERT_MESSAGE(std::string(token), threshold.has_value());
        CPPUNIT_ASSERT_EQUAL(std::string(token), threshold->toString());
    }
    for (std::string_view token : {"",
                                   "percent/0",
                                   "percent/101",
                                   "percent/050",
                                   "percent/+5",
                                   "percent/-1",
                                   "percent/5/",
                                   "percent/5 ",
                                   "percent/",
                                   "percent/4294967296",
                                   " percent/50",
                                   "Percent/50",
                                   "count/1",
                                   "count/2",
                                   "ratio/1"}) {
        CPPUNIT_ASSERT_MESSAGE(std::string(token), !ApprovalThreshold::parse(token).has_value());
    }
    CPPUNIT_ASSERT(ApprovalThreshold::parse("percent/50") == ApprovalThreshold::defaultValue());
}

void
ConversationRepositoryTest::testApprovalThresholdRequired()
{
    auto required = [](std::string_view token, std::size_t admins) {
        return ApprovalThreshold::parse(token)->required(admins);
    };
    // The default is more than half of the admins, as before thresholds existed
    const std::array<std::size_t, 6> byDefault {0, 1, 2, 2, 3, 3};
    for (std::size_t admins = 0; admins < byDefault.size(); ++admins)
        CPPUNIT_ASSERT_EQUAL(byDefault[admins], ApprovalThreshold::defaultValue().required(admins));
    CPPUNIT_ASSERT_EQUAL(std::size_t(2), required("percent/66", 3));
    CPPUNIT_ASSERT_EQUAL(std::size_t(4), required("percent/66", 6));
    // "percent/66" equals "at least two thirds" up to 52 admins, and needs one vote less from 53
    CPPUNIT_ASSERT_EQUAL(std::size_t(35), required("percent/66", 52));
    CPPUNIT_ASSERT_EQUAL(std::size_t(35), required("percent/66", 53));
    CPPUNIT_ASSERT_EQUAL(std::size_t(1), required("percent/1", 1));
    CPPUNIT_ASSERT_EQUAL(std::size_t(2), required("percent/1", 100));
    // Every threshold is capped at all admins
    CPPUNIT_ASSERT_EQUAL(std::size_t(4), required("percent/100", 4));
}

void
ConversationRepositoryTest::testRoleChangePermissions()
{
    const std::array<Permission, 2> adminOnly {Permission::ChangeMemberRole, Permission::ChangeApprovalThreshold};
    for (auto mode : {ConversationMode::INVITES_ONLY, ConversationMode::FEED, ConversationMode::DOCUMENT}) {
        for (auto permission : adminOnly) {
            CPPUNIT_ASSERT(hasPermission(mode, MemberRole::ADMIN, permission));
            CPPUNIT_ASSERT(!hasPermission(mode, MemberRole::MEMBER, permission));
            CPPUNIT_ASSERT(!hasPermission(mode, MemberRole::INVITED, permission));
            CPPUNIT_ASSERT(!hasPermission(mode, MemberRole::BANNED, permission));
        }
    }
    // A one-to-one conversation has no roles to change
    for (auto permission : adminOnly) {
        CPPUNIT_ASSERT(!hasPermission(ConversationMode::ONE_TO_ONE, MemberRole::ADMIN, permission));
        CPPUNIT_ASSERT(!hasPermission(ConversationMode::ONE_TO_ONE, MemberRole::MEMBER, permission));
    }
    CPPUNIT_ASSERT(permissionName(Permission::ChangeMemberRole) == "changeMemberRole");
    CPPUNIT_ASSERT(permissionName(Permission::ChangeApprovalThreshold) == "changeApprovalThreshold");
}

void
ConversationRepositoryTest::testAdminVotesAccepted()
{
    auto aliceUri = Manager::instance().getAccount<JamiAccount>(aliceId)->getUsername();
    auto bobUri = Manager::instance().getAccount<JamiAccount>(bobId)->getUsername();
    const std::string memberUri(40, 'c');
    auto swarm = handBuiltSwarm({bobUri}, {memberUri});

    // A ban vote keeps its format without an action
    writeFile(swarm, fmt::format("votes/ban/members/{}/{}", memberUri, aliceUri));
    CPPUNIT_ASSERT(commitValidates(swarm, CommitMessage::vote(memberUri), true));
    writeFile(swarm, fmt::format("votes/promote/members/{}/{}", memberUri, aliceUri));
    CPPUNIT_ASSERT(commitValidates(swarm, CommitMessage::vote(memberUri, CommitAction::PROMOTE), true));
    writeFile(swarm, fmt::format("votes/demote/admins/{}/{}", bobUri, aliceUri));
    CPPUNIT_ASSERT(commitValidates(swarm, CommitMessage::vote(bobUri, CommitAction::DEMOTE), true));
    writeFile(swarm, fmt::format("votes/approval/percent/66/{}", aliceUri));
    CPPUNIT_ASSERT(commitValidates(swarm, CommitMessage::approvalVote("percent/66"), true));
}

void
ConversationRepositoryTest::testInvalidVotesRejected()
{
    auto aliceUri = Manager::instance().getAccount<JamiAccount>(aliceId)->getUsername();
    auto bobUri = Manager::instance().getAccount<JamiAccount>(bobId)->getUsername();
    const std::string memberUri(40, 'c');
    const std::string otherUri(40, 'd');
    auto swarm = handBuiltSwarm({bobUri}, {memberUri});

    const std::vector<std::pair<std::string, CommitMessage>> cases {
        // A promotion targets a member, and a demotion an admin
        {fmt::format("votes/promote/admins/{}/{}", bobUri, aliceUri),
         CommitMessage::vote(bobUri, CommitAction::PROMOTE)},
        {fmt::format("votes/demote/members/{}/{}", memberUri, aliceUri),
         CommitMessage::vote(memberUri, CommitAction::DEMOTE)},
        // The path and the message name the same action and the same URI
        {fmt::format("votes/promote/members/{}/{}", memberUri, aliceUri),
         CommitMessage::vote(memberUri, CommitAction::DEMOTE)},
        {fmt::format("votes/promote/members/{}/{}", memberUri, aliceUri),
         CommitMessage::vote(otherUri, CommitAction::PROMOTE)},
        // A vote without an action is a ban or unban vote
        {fmt::format("votes/promote/members/{}/{}", memberUri, aliceUri), CommitMessage::vote(memberUri)},
        {fmt::format("votes/crown/members/{}/{}", memberUri, aliceUri), CommitMessage::vote(memberUri, "crown")},
        // Nobody votes on their own role or ban
        {fmt::format("votes/demote/admins/{}/{}", aliceUri, aliceUri),
         CommitMessage::vote(aliceUri, CommitAction::DEMOTE)},
        {fmt::format("votes/ban/admins/{}/{}", aliceUri, aliceUri), CommitMessage::vote(aliceUri)},
        // An approval vote names one canonical token in its path and its message, and its own voter
        {fmt::format("votes/approval/percent/66/{}", aliceUri), CommitMessage::approvalVote("percent/1")},
        {fmt::format("votes/approval/percent/050/{}", aliceUri), CommitMessage::approvalVote("percent/050")},
        {fmt::format("votes/approval/percent/66/{}", bobUri), CommitMessage::approvalVote("percent/66")},
    };
    for (const auto& [path, message] : cases) {
        writeFile(swarm, path);
        CPPUNIT_ASSERT_MESSAGE(path + " " + message.toString(), !commitValidates(swarm, message, false));
    }
}

void
ConversationRepositoryTest::testRoleActionsByMemberRejected()
{
    auto aliceUri = Manager::instance().getAccount<JamiAccount>(aliceId)->getUsername();
    auto bobUri = Manager::instance().getAccount<JamiAccount>(bobId)->getUsername();
    const std::string memberUri(40, 'c');
    auto swarm = handBuiltSwarm({bobUri}, {memberUri});
    // Alice is a plain member from here on, and Bob the only admin
    moveFile(swarm, fmt::format("admins/{}.crt", aliceUri), fmt::format("members/{}.crt", aliceUri));
    commitRaw(swarm, CommitMessage::text("Alice is a member"));

    writeFile(swarm, fmt::format("votes/promote/members/{}/{}", memberUri, aliceUri));
    CPPUNIT_ASSERT(!commitValidates(swarm, CommitMessage::vote(memberUri, CommitAction::PROMOTE), false));
    writeFile(swarm, fmt::format("votes/approval/percent/1/{}", aliceUri));
    CPPUNIT_ASSERT(!commitValidates(swarm, CommitMessage::approvalVote("percent/1"), false));
    moveFile(swarm, fmt::format("members/{}.crt", memberUri), fmt::format("admins/{}.crt", memberUri));
    CPPUNIT_ASSERT(!commitValidates(swarm, CommitMessage::member(CommitAction::PROMOTE, memberUri), false));
    writeFile(swarm, std::string(ApprovalThreshold::FILE_PATH), "percent/1");
    CPPUNIT_ASSERT(!commitValidates(swarm, CommitMessage::approvalChange("percent/1"), false));
}

void
ConversationRepositoryTest::testPromoteResolution()
{
    auto aliceUri = Manager::instance().getAccount<JamiAccount>(aliceId)->getUsername();
    auto bobUri = Manager::instance().getAccount<JamiAccount>(bobId)->getUsername();
    const std::string memberUri(40, 'c');
    auto swarm = handBuiltSwarm({bobUri}, {memberUri});
    auto votes = fmt::format("votes/promote/members/{}/", memberUri);
    writeFile(swarm, votes + aliceUri);
    writeFile(swarm, votes + bobUri);
    commitRaw(swarm, CommitMessage::vote(memberUri, CommitAction::PROMOTE));

    auto memberFile = fmt::format("members/{}.crt", memberUri);
    auto adminFile = fmt::format("admins/{}.crt", memberUri);
    auto promotion = CommitMessage::member(CommitAction::PROMOTE, memberUri);

    // One vote of two admins is not more than half
    removeFile(swarm, votes + aliceUri);
    moveFile(swarm, memberFile, adminFile);
    CPPUNIT_ASSERT(!commitValidates(swarm, promotion, false));

    // Another file changes along
    removeFile(swarm, votes + aliceUri);
    removeFile(swarm, votes + bobUri);
    moveFile(swarm, memberFile, adminFile);
    writeFile(swarm, "profile.vcf", "TITLE: Promoted\n");
    CPPUNIT_ASSERT(!commitValidates(swarm, promotion, false));

    // The certificate changes on the way
    removeFile(swarm, votes + aliceUri);
    removeFile(swarm, votes + bobUri);
    removeFile(swarm, memberFile);
    writeFile(swarm, adminFile, "not the member's certificate");
    CPPUNIT_ASSERT(!commitValidates(swarm, promotion, false));

    // Both admins voted and the certificate moved unchanged
    removeFile(swarm, votes + aliceUri);
    removeFile(swarm, votes + bobUri);
    moveFile(swarm, memberFile, adminFile);
    CPPUNIT_ASSERT(commitValidates(swarm, promotion, true));
    CPPUNIT_ASSERT(roleOf(swarm, memberUri) == MemberRole::ADMIN);
}

void
ConversationRepositoryTest::testDemoteResolution()
{
    auto aliceUri = Manager::instance().getAccount<JamiAccount>(aliceId)->getUsername();
    auto bobUri = Manager::instance().getAccount<JamiAccount>(bobId)->getUsername();
    const std::string carlUri(40, 'e');
    auto swarm = handBuiltSwarm({bobUri, carlUri}, {});
    auto votes = fmt::format("votes/demote/admins/{}/", carlUri);
    writeFile(swarm, votes + aliceUri);
    writeFile(swarm, votes + bobUri);
    commitRaw(swarm, CommitMessage::vote(carlUri, CommitAction::DEMOTE));

    auto adminFile = fmt::format("admins/{}.crt", carlUri);
    auto memberFile = fmt::format("members/{}.crt", carlUri);
    auto demotion = CommitMessage::member(CommitAction::DEMOTE, carlUri);

    // One vote of three admins
    removeFile(swarm, votes + aliceUri);
    moveFile(swarm, adminFile, memberFile);
    CPPUNIT_ASSERT(!commitValidates(swarm, demotion, false));

    // Two votes of three admins, the targeted admin counted in the electorate
    removeFile(swarm, votes + aliceUri);
    removeFile(swarm, votes + bobUri);
    moveFile(swarm, adminFile, memberFile);
    CPPUNIT_ASSERT(commitValidates(swarm, demotion, true));
    CPPUNIT_ASSERT(roleOf(swarm, carlUri) == MemberRole::MEMBER);
}

void
ConversationRepositoryTest::testStepDown()
{
    auto aliceUri = Manager::instance().getAccount<JamiAccount>(aliceId)->getUsername();
    auto bobUri = Manager::instance().getAccount<JamiAccount>(bobId)->getUsername();
    auto swarm = handBuiltSwarm({bobUri}, {});
    // Bob's pending vote on Alice, which stepping down must leave alone
    auto bobVote = fmt::format("votes/demote/admins/{}/{}", aliceUri, bobUri);
    writeFile(swarm, bobVote);
    commitRaw(swarm, CommitMessage::vote(aliceUri, CommitAction::DEMOTE));

    auto adminFile = fmt::format("admins/{}.crt", aliceUri);
    auto memberFile = fmt::format("members/{}.crt", aliceUri);
    auto stepDown = CommitMessage::member(CommitAction::DEMOTE, aliceUri);

    removeFile(swarm, bobVote);
    moveFile(swarm, adminFile, memberFile);
    CPPUNIT_ASSERT(!commitValidates(swarm, stepDown, false));

    // Alone and without a vote, like leaving
    moveFile(swarm, adminFile, memberFile);
    CPPUNIT_ASSERT(commitValidates(swarm, stepDown, true));
    CPPUNIT_ASSERT(roleOf(swarm, aliceUri) == MemberRole::MEMBER);
}

void
ConversationRepositoryTest::testDemoteLastAdminRejected()
{
    auto aliceUri = Manager::instance().getAccount<JamiAccount>(aliceId)->getUsername();
    auto swarm = handBuiltSwarm({}, {std::string(40, 'c')});
    moveFile(swarm, fmt::format("admins/{}.crt", aliceUri), fmt::format("members/{}.crt", aliceUri));
    CPPUNIT_ASSERT(!commitValidates(swarm, CommitMessage::member(CommitAction::DEMOTE, aliceUri), false));
}

void
ConversationRepositoryTest::testApprovalChange()
{
    auto aliceUri = Manager::instance().getAccount<JamiAccount>(aliceId)->getUsername();
    auto bobUri = Manager::instance().getAccount<JamiAccount>(bobId)->getUsername();
    const std::string carlUri(40, 'e');
    auto swarm = handBuiltSwarm({bobUri, carlUri}, {});
    const std::string policyFile {ApprovalThreshold::FILE_PATH};

    // Materializing the implicit default must not consume votes without changing the threshold
    auto defaultVotes = std::string("votes/approval/percent/50/");
    writeFile(swarm, defaultVotes + aliceUri);
    writeFile(swarm, defaultVotes + bobUri);
    commitRaw(swarm, CommitMessage::approvalVote("percent/50"));
    removeFile(swarm, defaultVotes + aliceUri);
    removeFile(swarm, defaultVotes + bobUri);
    writeFile(swarm, policyFile, "percent/50");
    CPPUNIT_ASSERT(!commitValidates(swarm, CommitMessage::approvalChange("percent/50"), false));

    auto votes = std::string("votes/approval/percent/1/");
    writeFile(swarm, votes + aliceUri);
    writeFile(swarm, votes + bobUri);
    commitRaw(swarm, CommitMessage::approvalVote("percent/1"));

    // One vote of three admins under the default threshold
    removeFile(swarm, votes + aliceUri);
    writeFile(swarm, policyFile, "percent/1");
    CPPUNIT_ASSERT(!commitValidates(swarm, CommitMessage::approvalChange("percent/1"), false));

    // The stored token differs from the commit's
    removeFile(swarm, votes + aliceUri);
    removeFile(swarm, votes + bobUri);
    writeFile(swarm, policyFile, "percent/66");
    CPPUNIT_ASSERT(!commitValidates(swarm, CommitMessage::approvalChange("percent/1"), false));

    // A stored token with a trailing newline is not canonical
    removeFile(swarm, votes + aliceUri);
    removeFile(swarm, votes + bobUri);
    writeFile(swarm, policyFile, "percent/1\n");
    CPPUNIT_ASSERT(!commitValidates(swarm, CommitMessage::approvalChange("percent/1"), false));

    // A policy commit without a known action
    removeFile(swarm, votes + aliceUri);
    removeFile(swarm, votes + bobUri);
    writeFile(swarm, policyFile, "percent/1");
    auto unknownPolicy = CommitMessage::approvalChange("percent/1");
    unknownPolicy.action = "other";
    CPPUNIT_ASSERT(!commitValidates(swarm, unknownPolicy, false));

    // Two votes of three admins
    removeFile(swarm, votes + aliceUri);
    removeFile(swarm, votes + bobUri);
    writeFile(swarm, policyFile, "percent/1");
    CPPUNIT_ASSERT(commitValidates(swarm, CommitMessage::approvalChange("percent/1"), true));

    // Consuming votes must change the stored policy, even when the token already matches
    writeFile(swarm, votes + aliceUri);
    writeFile(swarm, votes + bobUri);
    commitRaw(swarm, CommitMessage::approvalVote("percent/1"));
    removeFile(swarm, votes + aliceUri);
    removeFile(swarm, votes + bobUri);
    CPPUNIT_ASSERT(!commitValidates(swarm, CommitMessage::approvalChange("percent/1"), false));
}

void
ConversationRepositoryTest::testApprovalResolutionWriteFailure()
{
    auto swarm = handBuiltSwarm({}, {});
    CPPUNIT_ASSERT(!swarm.repository->voteApprovalThreshold("percent/1").empty());
    auto head = swarm.repository->getHead();
    auto pending = swarm.repository->votes();
    CPPUNIT_ASSERT_EQUAL(std::size_t(1), pending.size());
    auto policy = swarm.path / "policy";
    std::filesystem::create_directories(policy / "approval");
    auto resolution = swarm.repository->resolveApprovalThreshold("percent/1");
    std::filesystem::remove(policy / "approval");
    CPPUNIT_ASSERT(resolution.empty());
    CPPUNIT_ASSERT_EQUAL(head, swarm.repository->getHead());
    CPPUNIT_ASSERT(pending == swarm.repository->votes());
    CPPUNIT_ASSERT(!std::filesystem::exists(policy / "approval"));
    CPPUNIT_ASSERT(!swarm.repository->resolveApprovalThreshold("percent/1").empty());
    CPPUNIT_ASSERT(swarm.repository->votes().empty());
    CPPUNIT_ASSERT_EQUAL(std::string("percent/1"), swarm.repository->approvalThreshold()->toString());
}

void
ConversationRepositoryTest::testApprovalResolutionCommitFailure()
{
    auto swarm = handBuiltSwarm({}, {});
    auto lockPath = std::filesystem::path(git_repository_path(swarm.repo.get())) / "refs/heads/main.lock";
    for (const auto& token : {"percent/1", "percent/66"}) {
        auto policy = swarm.path / "policy/approval";
        auto hadPolicy = std::filesystem::exists(policy);
        auto original = hadPolicy ? fileContent(swarm, "policy/approval") : std::string();
        CPPUNIT_ASSERT(!swarm.repository->voteApprovalThreshold(token).empty());
        auto head = swarm.repository->getHead();
        auto pending = swarm.repository->votes();
        CPPUNIT_ASSERT_EQUAL(std::size_t(1), pending.size());
        std::ofstream(lockPath).close();
        auto resolution = swarm.repository->resolveApprovalThreshold(token);
        std::filesystem::remove(lockPath);
        CPPUNIT_ASSERT(resolution.empty());
        CPPUNIT_ASSERT_EQUAL(head, swarm.repository->getHead());
        CPPUNIT_ASSERT(pending == swarm.repository->votes());
        CPPUNIT_ASSERT_EQUAL(hadPolicy, std::filesystem::exists(policy));
        if (hadPolicy)
            CPPUNIT_ASSERT_EQUAL(original, fileContent(swarm, "policy/approval"));
        CPPUNIT_ASSERT(!swarm.repository->resolveApprovalThreshold(token).empty());
        CPPUNIT_ASSERT(swarm.repository->votes().empty());
        CPPUNIT_ASSERT_EQUAL(std::string(token), swarm.repository->approvalThreshold()->toString());
    }
}

void
ConversationRepositoryTest::testApprovalResolutionStagingFailure()
{
    auto swarm = handBuiltSwarm({}, {});
    auto lockPath = std::filesystem::path(git_repository_path(swarm.repo.get())) / "index.lock";
    for (const auto& token : {"percent/1", "percent/66"}) {
        auto policy = swarm.path / "policy/approval";
        auto hadPolicy = std::filesystem::exists(policy);
        auto original = hadPolicy ? fileContent(swarm, "policy/approval") : std::string();
        CPPUNIT_ASSERT(!swarm.repository->voteApprovalThreshold(token).empty());
        auto head = swarm.repository->getHead();
        auto pending = swarm.repository->votes();
        std::ofstream(lockPath).close();
        auto resolution = swarm.repository->resolveApprovalThreshold(token);
        std::filesystem::remove(lockPath);
        CPPUNIT_ASSERT(resolution.empty());
        CPPUNIT_ASSERT_EQUAL(head, swarm.repository->getHead());
        CPPUNIT_ASSERT(pending == swarm.repository->votes());
        CPPUNIT_ASSERT_EQUAL(hadPolicy, std::filesystem::exists(policy));
        if (hadPolicy)
            CPPUNIT_ASSERT_EQUAL(original, fileContent(swarm, "policy/approval"));
        CPPUNIT_ASSERT(!swarm.repository->resolveApprovalThreshold(token).empty());
        CPPUNIT_ASSERT(swarm.repository->votes().empty());
    }
}

void
ConversationRepositoryTest::testPendingVoteListing()
{
    auto alice = Manager::instance().getAccount<JamiAccount>(aliceId)->getUsername();
    const std::string bob(40, 'b');
    const std::string member(40, 'c');
    const std::string invited(40, 'd');
    const std::string banned(40, 'e');
    const std::string formerAdmin(40, 'f');
    auto swarm = handBuiltSwarm({bob}, {member});
    writeFile(swarm, "invited/" + invited);
    writeFile(swarm, "banned/members/" + banned + ".crt", fileContent(swarm, "members/" + member + ".crt"));
    const std::vector<std::tuple<std::string, std::string, std::string>> targets {{"ban", "members", member},
                                                                                  {"ban", "invited", invited},
                                                                                  {"unban", "members", banned},
                                                                                  {"promote", "members", member},
                                                                                  {"demote", "admins", bob},
                                                                                  {"approval", "", "percent/1"}};
    for (const auto& [action, type, target] : targets) {
        auto path = action == "approval" ? "votes/approval/" + target : "votes/" + action + "/" + type + "/" + target;
        writeFile(swarm, path + "/" + alice);
        writeFile(swarm, path + "/" + formerAdmin);
    }
    writeFile(swarm, "votes/promote/members/" + member + "/" + bob);
    writeFile(swarm, "votes/ban/members/" + formerAdmin + "/" + alice);
    writeFile(swarm, "votes/approval/percent/0/" + alice);
    commitRaw(swarm, CommitMessage::text("pending votes"));
    auto pending = swarm.repository->votes();
    CPPUNIT_ASSERT_EQUAL(targets.size(), pending.size());
    for (const auto& [action, type, target] : targets) {
        auto vote = std::find_if(pending.begin(), pending.end(), [&](const auto& entry) {
            return entry.at("action") == action && entry.at("type") == type && entry.at("target") == target;
        });
        CPPUNIT_ASSERT(vote != pending.end());
        auto voters = action == "promote" ? std::min(alice, bob) + "," + std::max(alice, bob) : alice;
        CPPUNIT_ASSERT_EQUAL(voters, vote->at("voters"));
        CPPUNIT_ASSERT_EQUAL(std::string("2"), vote->at("required"));
    }
}

void
ConversationRepositoryTest::testStoredThresholdApplies()
{
    auto aliceUri = Manager::instance().getAccount<JamiAccount>(aliceId)->getUsername();
    auto bobUri = Manager::instance().getAccount<JamiAccount>(bobId)->getUsername();
    const std::string carlUri(40, 'e');
    const std::string bannedUri(40, 'c');
    const std::string promotedUri(40, 'd');
    auto swarm = handBuiltSwarm({bobUri, carlUri}, {bannedUri, promotedUri}, "percent/1");

    // With "percent/1", one vote of three admins bans
    auto banVote = fmt::format("votes/ban/members/{}/{}", bannedUri, aliceUri);
    writeFile(swarm, banVote);
    commitRaw(swarm, CommitMessage::vote(bannedUri));
    removeFile(swarm, banVote);
    moveFile(swarm, fmt::format("members/{}.crt", bannedUri), fmt::format("banned/members/{}.crt", bannedUri));
    CPPUNIT_ASSERT(commitValidates(swarm, CommitMessage::member(CommitAction::BAN, bannedUri), true));

    // and promotes
    auto promoteVote = fmt::format("votes/promote/members/{}/{}", promotedUri, aliceUri);
    writeFile(swarm, promoteVote);
    commitRaw(swarm, CommitMessage::vote(promotedUri, CommitAction::PROMOTE));
    removeFile(swarm, promoteVote);
    moveFile(swarm, fmt::format("members/{}.crt", promotedUri), fmt::format("admins/{}.crt", promotedUri));
    CPPUNIT_ASSERT(commitValidates(swarm, CommitMessage::member(CommitAction::PROMOTE, promotedUri), true));
}

void
ConversationRepositoryTest::testUnanimousThreshold()
{
    auto aliceUri = Manager::instance().getAccount<JamiAccount>(aliceId)->getUsername();
    auto bobUri = Manager::instance().getAccount<JamiAccount>(bobId)->getUsername();
    const std::string carlUri(40, 'e');
    const std::string memberUri(40, 'c');
    auto swarm = handBuiltSwarm({bobUri, carlUri}, {memberUri}, "percent/100");
    auto votes = fmt::format("votes/promote/members/{}/", memberUri);
    for (const auto& voter : {aliceUri, bobUri, carlUri})
        writeFile(swarm, votes + voter);
    commitRaw(swarm, CommitMessage::vote(memberUri, CommitAction::PROMOTE));
    auto memberFile = fmt::format("members/{}.crt", memberUri);
    auto adminFile = fmt::format("admins/{}.crt", memberUri);
    auto promotion = CommitMessage::member(CommitAction::PROMOTE, memberUri);

    // Two votes of three admins
    removeFile(swarm, votes + aliceUri);
    removeFile(swarm, votes + bobUri);
    moveFile(swarm, memberFile, adminFile);
    CPPUNIT_ASSERT(!commitValidates(swarm, promotion, false));

    for (const auto& voter : {aliceUri, bobUri, carlUri})
        removeFile(swarm, votes + voter);
    moveFile(swarm, memberFile, adminFile);
    CPPUNIT_ASSERT(commitValidates(swarm, promotion, true));
}

void
ConversationRepositoryTest::testInvalidStoredThresholdRejectsVotes()
{
    auto aliceUri = Manager::instance().getAccount<JamiAccount>(aliceId)->getUsername();
    const std::string memberUri(40, 'c');
    // Alice alone would ban under any valid threshold
    auto swarm = handBuiltSwarm({}, {memberUri}, "percent/0");
    auto vote = fmt::format("votes/ban/members/{}/{}", memberUri, aliceUri);
    writeFile(swarm, vote);
    commitRaw(swarm, CommitMessage::vote(memberUri));
    removeFile(swarm, vote);
    moveFile(swarm, fmt::format("members/{}.crt", memberUri), fmt::format("banned/members/{}.crt", memberUri));
    CPPUNIT_ASSERT(!commitValidates(swarm, CommitMessage::member(CommitAction::BAN, memberUri), false));
}

void
ConversationRepositoryTest::testBannedWinsOverAdmin()
{
    auto aliceUri = Manager::instance().getAccount<JamiAccount>(aliceId)->getUsername();
    auto bobUri = Manager::instance().getAccount<JamiAccount>(bobId)->getUsername();
    auto swarm = handBuiltSwarm({bobUri}, {});
    // What a merge of a ban and a concurrent promotion may leave: Alice is banned and admin
    auto adminFile = fmt::format("admins/{}.crt", aliceUri);
    writeFile(swarm, fmt::format("banned/members/{}.crt", aliceUri), fileContent(swarm, adminFile));
    commitRaw(swarm, CommitMessage::text("Alice banned"));

    writeFile(swarm, "profile.vcf", "TITLE: Banned admin\n");
    CPPUNIT_ASSERT(!commitValidates(swarm, CommitMessage::updateProfile(), false));
    CPPUNIT_ASSERT(roleOf(swarm, aliceUri) == MemberRole::BANNED);
}

void
ConversationRepositoryTest::testBannedWinsOverMember()
{
    auto aliceUri = Manager::instance().getAccount<JamiAccount>(aliceId)->getUsername();
    auto bobUri = Manager::instance().getAccount<JamiAccount>(bobId)->getUsername();
    auto swarm = handBuiltSwarm({bobUri}, {});
    auto memberFile = fmt::format("members/{}.crt", aliceUri);
    moveFile(swarm, fmt::format("admins/{}.crt", aliceUri), memberFile);
    commitRaw(swarm, CommitMessage::text("Alice demoted"));
    CPPUNIT_ASSERT(commitValidates(swarm, CommitMessage::text("hello"), false));

    // What a merge of a ban and a concurrent demotion may leave: Alice is banned and member
    writeFile(swarm, fmt::format("banned/admins/{}.crt", aliceUri), fileContent(swarm, memberFile));
    commitRaw(swarm, CommitMessage::text("Alice banned"));

    CPPUNIT_ASSERT(!commitValidates(swarm, CommitMessage::text("hello"), false));
    CPPUNIT_ASSERT(roleOf(swarm, aliceUri) == MemberRole::BANNED);
}

void
ConversationRepositoryTest::testMergeApprovalWithConflict()
{
    auto swarm = handBuiltSwarm({}, {});
    const std::string policyFile {ApprovalThreshold::FILE_PATH};
    git_object* basePtr = nullptr;
    CPPUNIT_ASSERT_EQUAL(0, git_revparse_single(&basePtr, swarm.repo.get(), "refs/heads/main"));
    GitObject base {basePtr};

    // Both branches set a first threshold, so the file has no common ancestor
    writeFile(swarm, policyFile, "percent/1");
    auto mainId = commitRaw(swarm, CommitMessage::approvalChange("percent/1"));

    git_reference* branchPtr = nullptr;
    CPPUNIT_ASSERT_EQUAL(0,
                         git_branch_create(&branchPtr,
                                           swarm.repo.get(),
                                           "to_merge",
                                           reinterpret_cast<git_commit*>(base.get()),
                                           false));
    git_reference_free(branchPtr);
    CPPUNIT_ASSERT_EQUAL(0, git_repository_set_head(swarm.repo.get(), "refs/heads/to_merge"));
    CPPUNIT_ASSERT_EQUAL(0, git_reset(swarm.repo.get(), base.get(), GIT_RESET_HARD, nullptr));
    writeFile(swarm, policyFile, "percent/66");
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto branchId = addCommit(swarm.repo.get(),
                              aliceAccount,
                              "to_merge",
                              CommitMessage::approvalChange("percent/66").toString());
    CPPUNIT_ASSERT(!branchId.empty());

    git_object* mainPtr = nullptr;
    CPPUNIT_ASSERT_EQUAL(0, git_repository_set_head(swarm.repo.get(), "refs/heads/main"));
    CPPUNIT_ASSERT_EQUAL(0, git_revparse_single(&mainPtr, swarm.repo.get(), "refs/heads/main"));
    GitObject mainCommit {mainPtr};
    CPPUNIT_ASSERT_EQUAL(0, git_reset(swarm.repo.get(), mainCommit.get(), GIT_RESET_HARD, nullptr));

    // Like a profile conflict, the higher commit id wins
    CPPUNIT_ASSERT(swarm.repository->merge(branchId).first);
    CPPUNIT_ASSERT_EQUAL(branchId > mainId ? std::string("percent/66") : std::string("percent/1"),
                         fileContent(swarm, policyFile));
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::ConversationRepositoryTest::name())
