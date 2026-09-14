/*
 * Copyright (C) 2026 Savoir-faire Linux Inc.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "jamidht/account_metadata.h"
#include "../../test_runner.h"

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <fstream>
#include <limits>
#include <thread>

#ifdef JAMI_TEST_WRAP_FSYNC
#include <cerrno>
#include <sys/stat.h>
namespace {
thread_local bool failNextDirectorySync = false;
}
extern "C" int __real_fsync(int fd);
extern "C" int
__wrap_fsync(int fd)
{
    struct stat status
    {};
    if (failNextDirectorySync && fstat(fd, &status) == 0 && S_ISDIR(status.st_mode)) {
        failNextDirectorySync = false;
        errno = EIO;
        return -1;
    }
    return __real_fsync(fd);
}
#endif

namespace jami::test {

class AccountMetadataTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "account_metadata"; }
    void setUp() override
    {
        ownsRoot_ = std::filesystem::create_directory(root_);
        CPPUNIT_ASSERT(ownsRoot_);
    }
    void tearDown() override
    {
        if (ownsRoot_)
            std::filesystem::remove_all(root_);
    }

private:
    const std::filesystem::path root_ {"account-metadata-test-state"};
    bool ownsRoot_ {false};
    const std::string alice_ = std::string(64, 'a');
    const std::string bob_ = std::string(64, 'b');

    void testPatchAndReload()
    {
        AccountMetadataStore store(root_ / "a");
        CPPUNIT_ASSERT(store.values().empty());
        auto first = store.update(alice_, {{"name", "Président"}, {"empty", ""}});
        CPPUNIT_ASSERT(first.changed && first.valuesChanged);
        auto before = store.state();
        auto noop = store.update(alice_, {{"name", "Président"}});
        CPPUNIT_ASSERT(!noop.changed && !noop.valuesChanged);
        CPPUNIT_ASSERT(store.state() == before);
        store.update(alice_, {{"other", "second"}});
        AccountMetadataStore reload(root_ / "a");
        CPPUNIT_ASSERT(reload.values() == store.values());
        CPPUNIT_ASSERT(reload.state() == store.state());
        reload.update(bob_, {{"name", "renamed"}});
        CPPUNIT_ASSERT(reload.state().entries.at("name").revision > before.clock);
        CPPUNIT_ASSERT_EQUAL(std::string("second"), reload.values().at("other"));
    }

    void testDeterministicConcurrentMerge()
    {
        AccountMetadataStore a(root_ / "a"), b(root_ / "b"), c(root_ / "c");
        a.update(alice_, {{"same", "Alice"}, {"only-a", "1"}});
        b.update(bob_, {{"same", "Bob"}, {"only-b", "1"}});
        auto as = a.state(), bs = b.state();
        a.merge(bs);
        b.merge(as);
        CPPUNIT_ASSERT(a.state() == b.state());
        CPPUNIT_ASSERT_EQUAL(std::string("Bob"), a.values().at("same"));
        CPPUNIT_ASSERT_EQUAL(size_t(3), a.values().size());
        CPPUNIT_ASSERT(!a.merge(b.state()).changed);
        CPPUNIT_ASSERT(!b.merge(a.state()).changed);
        // A previously offline third device receives changes learned by an intermediary.
        c.merge(a.state());
        CPPUNIT_ASSERT(c.state() == b.state());
        c.update(alice_, {{"same", "causally newer"}});
        b.merge(c.state());
        a.merge(b.state());
        CPPUNIT_ASSERT(a.state() == c.state());
    }

    void testRemovalMarkersSurviveStalePeer()
    {
        AccountMetadataStore a(root_ / "a"), b(root_ / "b");
        const std::string member = "jami.channels.v1/stable/members/uri";
        const std::string deleted = "jami.channels.v1/stable/deleted";
        a.update(alice_, {{member, "1"}, {"unrelated", "kept"}});
        b.merge(a.state());
        auto stale = b.state();
        a.update(alice_, {{member, "0"}, {deleted, "1"}});
        b.merge(a.state());
        CPPUNIT_ASSERT(!a.merge(stale).changed);
        CPPUNIT_ASSERT_EQUAL(std::string("0"), b.values().at(member));
        CPPUNIT_ASSERT_EQUAL(std::string("1"), b.values().at(deleted));
        CPPUNIT_ASSERT_EQUAL(std::string("kept"), b.values().at("unrelated"));
        AccountMetadataStore reload(root_ / "b");
        CPPUNIT_ASSERT(reload.state() == a.state());
    }

    void testStampOnlyMergeAndClock()
    {
        AccountMetadataStore a(root_ / "a"), b(root_ / "b");
        a.update(alice_, {{"name", "same"}});
        b.update(bob_, {{"name", "same"}});
        auto change = a.merge(b.state());
        CPPUNIT_ASSERT(change.changed);
        CPPUNIT_ASSERT(!change.valuesChanged);
        auto clockOnly = a.state();
        clockOnly.clock = 200;
        CPPUNIT_ASSERT(!a.merge(clockOnly).changed);
        AccountMetadataStore reload(root_ / "a");
        reload.update(alice_, {{"next", "new"}});
        CPPUNIT_ASSERT_EQUAL(uint64_t(201), reload.state().entries.at("next").revision);
    }

    void testAtomicLegacyImport()
    {
        AccountMetadataStore local(root_ / "local"), remote(root_ / "remote");
        const std::string member = "jami.channels.v1/stable/members/uri";
        const std::string name = "jami.channels.v1/stable/name";
        const std::string deleted = "jami.channels.v1/removed/deleted";
        CPPUNIT_ASSERT(local.values().empty()); // stale Android migration snapshot
        const std::map<std::string, std::string> legacy {{member, "1"}, {name, "Legacy channel"}, {deleted, "0"}};
        remote.update(bob_, {{member, "0"}, {deleted, "1"}});
        local.merge(remote.state()); // arrives after the snapshot but before native PATCH
        auto before = local.state();
        auto imported = local.update(alice_, legacy, true);
        CPPUNIT_ASSERT(imported.changed && imported.valuesChanged);
        CPPUNIT_ASSERT(local.state().entries.at(member) == before.entries.at(member));
        CPPUNIT_ASSERT(local.state().entries.at(deleted) == before.entries.at(deleted));
        CPPUNIT_ASSERT_EQUAL(std::string("Legacy channel"), local.values().at(name));
        CPPUNIT_ASSERT_EQUAL(before.clock + 1, local.state().entries.at(name).revision);
        auto after = local.state();
        CPPUNIT_ASSERT(!local.update(alice_, legacy, true).changed);
        CPPUNIT_ASSERT(local.state() == after);
        AccountMetadataStore reload(root_ / "local");
        CPPUNIT_ASSERT(reload.state() == after);
        CPPUNIT_ASSERT(local.update(alice_, {{member, "1"}}).changed); // ordinary edits still PATCH
    }

    void testInvalidInputIsAtomic()
    {
        AccountMetadataStore a(root_ / "a");
        a.update(alice_, {{"name", "original"}});
        auto before = a.state();
        CPPUNIT_ASSERT_THROW(a.update(alice_, {{"", "bad"}, {"name", "changed"}}), std::invalid_argument);
        CPPUNIT_ASSERT_THROW(a.update(alice_, {{"name", std::string(65537, 'x')}}), std::invalid_argument);
        CPPUNIT_ASSERT_THROW(a.update(alice_, {{std::string(1025, 'x'), "v"}}), std::invalid_argument);
        CPPUNIT_ASSERT_THROW(a.update(alice_, {{"name", "\xc0\xaf"}}), std::invalid_argument);
        CPPUNIT_ASSERT_THROW(a.update(alice_, {{"name", std::string("x\0y", 3)}}), std::invalid_argument);
        CPPUNIT_ASSERT_THROW(a.update("not-a-device", {{"name", "value"}}), std::invalid_argument);
        auto conflicting = before;
        conflicting.entries.at("name").value = "forged";
        CPPUNIT_ASSERT_THROW(a.merge(conflicting), std::invalid_argument);
        auto invalid = before;
        invalid.entries.at("name").revision = 0;
        CPPUNIT_ASSERT_THROW(a.merge(invalid), std::invalid_argument);
        CPPUNIT_ASSERT(a.state() == before);
        std::map<std::string, std::string> oversized;
        for (unsigned i = 0; i < 65; ++i)
            oversized[std::to_string(i)] = std::string(65536, 'x');
        CPPUNIT_ASSERT_THROW(a.update(alice_, oversized), std::invalid_argument);
        CPPUNIT_ASSERT(a.state() == before);
    }

    void testCorruptionAndWriteFailure()
    {
        {
            std::ofstream file(root_ / "corrupt");
            file << "corrupt";
        }
        AccountMetadataStore corrupt(root_ / "corrupt");
        CPPUNIT_ASSERT_THROW(corrupt.values(), std::exception);
        CPPUNIT_ASSERT_THROW(corrupt.update(alice_, {{"name", "lost"}}), std::exception);
        CPPUNIT_ASSERT_EQUAL(uintmax_t(7), std::filesystem::file_size(root_ / "corrupt"));
        CPPUNIT_ASSERT_THROW(AccountMetadataStore::decode(std::string_view("\x80", 1)), std::exception);
        msgpack::sbuffer packed;
        msgpack::pack(packed, AccountMetadata {});
        CPPUNIT_ASSERT(AccountMetadataStore::decode({packed.data(), packed.size()}).entries.empty());
        packed.write("\x00", 1);
        CPPUNIT_ASSERT_THROW(AccountMetadataStore::decode({packed.data(), packed.size()}), std::exception);
        AccountMetadataStore missing(root_ / "missing" / "state");
        CPPUNIT_ASSERT_THROW(missing.update(alice_, {{"name", "lost"}}), std::exception);
        CPPUNIT_ASSERT(missing.values().empty());
        AccountMetadataStore a(root_ / "a");
        a.update(alice_, {{"name", "durable"}});
        auto before = a.state();
        std::filesystem::create_directory(root_ / "a.new");
        CPPUNIT_ASSERT_THROW(a.update(alice_, {{"name", "not saved"}}), std::exception);
        CPPUNIT_ASSERT(a.state() == before);
        AccountMetadataStore reload(root_ / "a");
        CPPUNIT_ASSERT(reload.state() == before);
    }

    void testOverflowAndConcurrentUpdates()
    {
        AccountMetadataStore a(root_ / "a");
        std::thread first([&] {
            for (int i = 0; i < 20; ++i)
                a.update(alice_, {{"a" + std::to_string(i), "1"}});
        });
        std::thread second([&] {
            for (int i = 0; i < 20; ++i)
                a.update(alice_, {{"b" + std::to_string(i), "1"}});
        });
        first.join();
        second.join();
        CPPUNIT_ASSERT_EQUAL(size_t(40), a.values().size());
        CPPUNIT_ASSERT_EQUAL(uint64_t(40), a.state().clock);
        auto exhausted = a.state();
        exhausted.clock = std::numeric_limits<uint64_t>::max();
        auto before = a.state();
        CPPUNIT_ASSERT_THROW(a.merge(exhausted), std::invalid_argument);
        CPPUNIT_ASSERT(a.state() == before);
        a.update(alice_, {{"new", "still writable"}});
        CPPUNIT_ASSERT_EQUAL(uint64_t(41), a.state().clock);
    }

    void testRemoteClockBoundsAreAtomic()
    {
        AccountMetadataStore store(root_ / "clock");
        store.update(alice_, {{"name", "original"}});
        const auto before = store.state();
        for (auto clock : {std::numeric_limits<uint64_t>::max(),
                           std::numeric_limits<uint64_t>::max() - 1,
                           before.clock + AccountMetadataStore::MAX_REMOTE_CLOCK_ADVANCE + 1}) {
            auto remote = before;
            remote.clock = clock;
            remote.entries["name"] = {clock, bob_, "untrusted clock"};
            CPPUNIT_ASSERT_THROW(store.merge(remote), std::invalid_argument);
            CPPUNIT_ASSERT(store.state() == before);
            AccountMetadataStore reloaded(root_ / "clock");
            CPPUNIT_ASSERT(reloaded.state() == before);
        }
        AccountMetadata clockOnly;
        clockOnly.clock = std::numeric_limits<uint64_t>::max();
        CPPUNIT_ASSERT_THROW(store.merge(clockOnly), std::invalid_argument);
        msgpack::sbuffer packed;
        msgpack::pack(packed, clockOnly);
        CPPUNIT_ASSERT_THROW(AccountMetadataStore::decode({packed.data(), packed.size()}), std::invalid_argument);
        // Imported states use the same merge bound even when their encoding is otherwise valid.
        clockOnly.clock = AccountMetadataStore::MAX_REMOTE_CLOCK_ADVANCE + before.clock + 1;
        packed.clear();
        msgpack::pack(packed, clockOnly);
        auto decoded = AccountMetadataStore::decode({packed.data(), packed.size()});
        CPPUNIT_ASSERT_THROW(store.merge(decoded), std::invalid_argument);
        store.update(alice_, {{"after", "writable"}});
        CPPUNIT_ASSERT_EQUAL(before.clock + 1, store.state().clock);
    }

    void testAcceptedClockBoundaryLeavesRoomForLocalUpdates()
    {
        AccountMetadataStore store(root_ / "boundary");
        AccountMetadata remote;
        remote.clock = AccountMetadataStore::MAX_REMOTE_CLOCK_ADVANCE;
        remote.entries["remote"] = {remote.clock, bob_, "received"};
        store.merge(remote);
        store.update(alice_, {{"local-a", "a"}, {"local-b", "b"}});
        CPPUNIT_ASSERT_EQUAL(remote.clock + 2, store.state().clock);
        AccountMetadataStore reloaded(root_ / "boundary");
        reloaded.update(alice_, {{"after-reload", "c"}});
        CPPUNIT_ASSERT_EQUAL(remote.clock + 3, reloaded.state().clock);
    }

#ifdef JAMI_TEST_WRAP_FSYNC
    void testPostRenameSyncFailureReloadsVisibleState()
    {
        AccountMetadataStore store(root_ / "durability");
        store.update(alice_, {{"name", "before"}});
        const auto before = store.state();
        failNextDirectorySync = true;
        CPPUNIT_ASSERT_THROW(store.update(alice_, {{"name", "after"}}), std::system_error);
        CPPUNIT_ASSERT(!failNextDirectorySync);
        AccountMetadataStore disk(root_ / "durability");
        CPPUNIT_ASSERT_EQUAL(std::string("after"), disk.values().at("name"));
        CPPUNIT_ASSERT(disk.state().clock > before.clock);
        // The failed write invalidates the cache; reads/sync must see the renamed file.
        CPPUNIT_ASSERT(store.state() == disk.state());
        store.update(alice_, {{"unrelated", "kept"}});
        AccountMetadataStore reloaded(root_ / "durability");
        CPPUNIT_ASSERT_EQUAL(std::string("after"), reloaded.values().at("name"));
        CPPUNIT_ASSERT_EQUAL(std::string("kept"), reloaded.values().at("unrelated"));
    }
#endif

    CPPUNIT_TEST_SUITE(AccountMetadataTest);
    CPPUNIT_TEST(testPatchAndReload);
    CPPUNIT_TEST(testDeterministicConcurrentMerge);
    CPPUNIT_TEST(testRemovalMarkersSurviveStalePeer);
    CPPUNIT_TEST(testStampOnlyMergeAndClock);
    CPPUNIT_TEST(testAtomicLegacyImport);
    CPPUNIT_TEST(testInvalidInputIsAtomic);
    CPPUNIT_TEST(testCorruptionAndWriteFailure);
    CPPUNIT_TEST(testOverflowAndConcurrentUpdates);
    CPPUNIT_TEST(testRemoteClockBoundsAreAtomic);
    CPPUNIT_TEST(testAcceptedClockBoundaryLeavesRoomForLocalUpdates);
#ifdef JAMI_TEST_WRAP_FSYNC
    CPPUNIT_TEST(testPostRenameSyncFailureReloadsVisibleState);
#endif
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(AccountMetadataTest, AccountMetadataTest::name());
} // namespace jami::test

CORE_TEST_RUNNER(jami::test::AccountMetadataTest::name())
