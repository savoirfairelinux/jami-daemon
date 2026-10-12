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

#include <filesystem>
#include <git2.h>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "fileutils.h"
#include "jamidht/conversationrepository.h"

#include "../../test_runner.h"

namespace jami {
namespace test {

class RepackTest : public CppUnit::TestFixture
{
public:
    ~RepackTest() = default;
    static std::string name() { return "repack"; }

    void setUp() override;
    void tearDown() override;

private:
    void testFewPacksAreLeftAlone();
    void testSmallPacksAreMerged();
    void testBigPackIsKept();
    void testManyPacksAreMergedInBatches();

    CPPUNIT_TEST_SUITE(RepackTest);
    CPPUNIT_TEST(testFewPacksAreLeftAlone);
    CPPUNIT_TEST(testSmallPacksAreMerged);
    CPPUNIT_TEST(testBigPackIsKept);
    CPPUNIT_TEST(testManyPacksAreMergedInBatches);
    CPPUNIT_TEST_SUITE_END();

    // Commit a message, then store it in a pack of its own, as a fetch does
    void commitInPack(const std::string& message);
    // Store objects in a pack of their own, then drop them from the loose objects
    void writePack(const std::vector<git_oid>& objects, const git_oid* commit = nullptr);
    std::set<std::string> packNames() const;
    void checkHistory(std::size_t commits) const;

    std::filesystem::path repoPath_ {};
    std::filesystem::path objectsPath_ {};
    git_repository* repo_ {nullptr};
    git_oid head_ {};
    std::size_t commits_ {0};
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(RepackTest, RepackTest::name());

void
RepackTest::setUp()
{
    git_libgit2_init();
    repoPath_ = fileutils::get_data_dir() / "repacktest";
    std::filesystem::remove_all(repoPath_);
    std::filesystem::create_directories(repoPath_);
    CPPUNIT_ASSERT_EQUAL(0, git_repository_init(&repo_, repoPath_.string().c_str(), false));
    objectsPath_ = repoPath_ / ".git" / "objects";
    commits_ = 0;
}

void
RepackTest::tearDown()
{
    git_repository_free(repo_);
    repo_ = nullptr;
    std::error_code ec;
    std::filesystem::remove_all(repoPath_, ec);
    git_libgit2_shutdown();
}

void
RepackTest::writePack(const std::vector<git_oid>& objects, const git_oid* commit)
{
    git_packbuilder* pb = nullptr;
    CPPUNIT_ASSERT_EQUAL(0, git_packbuilder_new(&pb, repo_));
    for (const auto& id : objects)
        CPPUNIT_ASSERT_EQUAL(0, git_packbuilder_insert(pb, &id, nullptr));
    if (commit)
        CPPUNIT_ASSERT_EQUAL(0, git_packbuilder_insert_commit(pb, commit));
    CPPUNIT_ASSERT_EQUAL(0, git_packbuilder_write(pb, (objectsPath_ / "pack").string().c_str(), 0, nullptr, nullptr));
    git_packbuilder_free(pb);
    // Only the packs may hold the objects, for the checks to mean anything
    for (const auto& entry : std::filesystem::directory_iterator(objectsPath_))
        if (entry.path().filename().string().size() == 2)
            std::filesystem::remove_all(entry.path());
}

void
RepackTest::commitInPack(const std::string& message)
{
    git_oid blob, treeId, commit;
    CPPUNIT_ASSERT_EQUAL(0, git_blob_create_from_buffer(&blob, repo_, message.data(), message.size()));
    git_treebuilder* builder = nullptr;
    CPPUNIT_ASSERT_EQUAL(0, git_treebuilder_new(&builder, repo_, nullptr));
    CPPUNIT_ASSERT_EQUAL(0, git_treebuilder_insert(nullptr, builder, "message", &blob, GIT_FILEMODE_BLOB));
    CPPUNIT_ASSERT_EQUAL(0, git_treebuilder_write(&treeId, builder));
    git_treebuilder_free(builder);

    git_signature* sig = nullptr;
    CPPUNIT_ASSERT_EQUAL(0, git_signature_new(&sig, "test", "test@jami.net", 1700000000, 0));
    git_tree* tree = nullptr;
    CPPUNIT_ASSERT_EQUAL(0, git_tree_lookup(&tree, repo_, &treeId));
    git_commit* parent = nullptr;
    if (commits_)
        CPPUNIT_ASSERT_EQUAL(0, git_commit_lookup(&parent, repo_, &head_));
    CPPUNIT_ASSERT_EQUAL(0,
                         git_commit_create_v(&commit,
                                             repo_,
                                             "refs/heads/main",
                                             sig,
                                             sig,
                                             nullptr,
                                             message.c_str(),
                                             tree,
                                             parent ? 1 : 0,
                                             parent));
    git_commit_free(parent);
    git_tree_free(tree);
    git_signature_free(sig);
    head_ = commit;
    ++commits_;
    writePack({}, &commit);
}

std::set<std::string>
RepackTest::packNames() const
{
    std::set<std::string> names;
    for (const auto& entry : std::filesystem::directory_iterator(objectsPath_ / "pack"))
        if (entry.path().extension() == ".pack")
            names.emplace(entry.path().stem().string());
    return names;
}

void
RepackTest::checkHistory(std::size_t commits) const
{
    // Read everything through a repository that never saw the old packs
    git_repository* repo = nullptr;
    CPPUNIT_ASSERT_EQUAL(0, git_repository_open(&repo, repoPath_.string().c_str()));
    git_revwalk* walk = nullptr;
    CPPUNIT_ASSERT_EQUAL(0, git_revwalk_new(&walk, repo));
    CPPUNIT_ASSERT_EQUAL(0, git_revwalk_push(walk, &head_));
    std::size_t count = 0;
    git_oid id;
    while (git_revwalk_next(&id, walk) == 0) {
        git_commit* commit = nullptr;
        CPPUNIT_ASSERT_EQUAL(0, git_commit_lookup(&commit, repo, &id));
        git_tree* tree = nullptr;
        CPPUNIT_ASSERT_EQUAL(0, git_commit_tree(&tree, commit));
        git_blob* blob = nullptr;
        CPPUNIT_ASSERT_EQUAL(0, git_blob_lookup(&blob, repo, git_tree_entry_id(git_tree_entry_byname(tree, "message"))));
        CPPUNIT_ASSERT_EQUAL(std::string(git_commit_message(commit)),
                             std::string(static_cast<const char*>(git_blob_rawcontent(blob)), git_blob_rawsize(blob)));
        git_blob_free(blob);
        git_tree_free(tree);
        git_commit_free(commit);
        ++count;
    }
    git_revwalk_free(walk);
    git_repository_free(repo);
    CPPUNIT_ASSERT_EQUAL(commits, count);
}

void
RepackTest::testFewPacksAreLeftAlone()
{
    for (auto i = 0; i < 10; ++i)
        commitInPack("message " + std::to_string(i));
    auto packs = packNames();
    CPPUNIT_ASSERT_EQUAL(std::size_t(10), packs.size());

    CPPUNIT_ASSERT_EQUAL(std::size_t(0), ConversationRepository::repack(repoPath_, 50));
    CPPUNIT_ASSERT(packNames() == packs);
}

void
RepackTest::testSmallPacksAreMerged()
{
    // An object no commit references is kept too: merging packs never prunes
    git_oid orphan;
    CPPUNIT_ASSERT_EQUAL(0, git_blob_create_from_buffer(&orphan, repo_, "orphan", 6));
    writePack({orphan});
    for (auto i = 0; i < 60; ++i)
        commitInPack("message " + std::to_string(i));
    CPPUNIT_ASSERT_EQUAL(std::size_t(61), packNames().size());

    CPPUNIT_ASSERT_EQUAL(std::size_t(61), ConversationRepository::repack(repoPath_, 50));
    CPPUNIT_ASSERT_EQUAL(std::size_t(1), packNames().size());
    checkHistory(60);
    git_repository* repo = nullptr;
    CPPUNIT_ASSERT_EQUAL(0, git_repository_open(&repo, repoPath_.string().c_str()));
    git_odb* odb = nullptr;
    CPPUNIT_ASSERT_EQUAL(0, git_repository_odb(&odb, repo));
    CPPUNIT_ASSERT(git_odb_exists(odb, &orphan));
    git_odb_free(odb);
    git_repository_free(repo);
}

void
RepackTest::testBigPackIsKept()
{
    // A pack far bigger than all the others together is not rewritten for them
    std::mt19937 rng(42);
    std::vector<git_oid> blobs;
    for (auto i = 0; i < 100; ++i) {
        std::string content(4096, '\0');
        for (auto& c : content)
            c = static_cast<char>(rng());
        git_oid id;
        CPPUNIT_ASSERT_EQUAL(0, git_blob_create_from_buffer(&id, repo_, content.data(), content.size()));
        blobs.emplace_back(id);
    }
    writePack(blobs);
    auto bigPack = *packNames().begin();
    for (auto i = 0; i < 55; ++i)
        commitInPack("message " + std::to_string(i));

    CPPUNIT_ASSERT_EQUAL(std::size_t(55), ConversationRepository::repack(repoPath_, 50));
    auto packs = packNames();
    CPPUNIT_ASSERT_EQUAL(std::size_t(2), packs.size());
    CPPUNIT_ASSERT(packs.count(bigPack));
    checkHistory(55);
}

void
RepackTest::testManyPacksAreMergedInBatches()
{
    // More packs than a merge takes at once: three batches, then their packs
    for (auto i = 0; i < 150; ++i)
        commitInPack("message " + std::to_string(i));

    CPPUNIT_ASSERT_EQUAL(std::size_t(153), ConversationRepository::repack(repoPath_, 2));
    CPPUNIT_ASSERT_EQUAL(std::size_t(1), packNames().size());
    checkHistory(150);
}

} // namespace test
} // namespace jami

JAMI_TEST_RUNNER(jami::test::RepackTest::name())
