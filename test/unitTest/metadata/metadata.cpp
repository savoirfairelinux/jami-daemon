/*
 * Copyright (C) 2026 Savoir-faire Linux Inc.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "metadata.h"
#include "jamidht/timestamp.h"
#include "../../test_runner.h"

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <fstream>
#include <limits>
#include <thread>

namespace jami::test {

class MetadataTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "metadata"; }
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
    const std::filesystem::path root_ {"metadata-test-state"};
    bool ownsRoot_ {false};
    const std::string alice_ = std::string(64, 'a');
    const std::string bob_ = std::string(64, 'b');

    void testWallTimeTimestamps()
    {
        MetadataStore store(root_ / "timestamps");
        const auto before = toMillisecondsSinceEpoch(nowMs());
        store.update(alice_, {{"name", "first"}, {"other", "value"}});
        const auto after = toMillisecondsSinceEpoch(nowMs());
        for (const auto& [key, entry] : store.state().entries) {
            CPPUNIT_ASSERT(entry.timestampMs >= before);
            CPPUNIT_ASSERT(entry.timestampMs <= after);
        }
    }

    void testClockRollbackAndKeyIndependence()
    {
        Metadata original;
        const auto future = toMillisecondsSinceEpoch(nowMs()) + 3600000;
        original.entries["name"] = {future, alice_, "before rollback"};
        {
            std::ofstream file(root_ / "rollback", std::ios::binary);
            msgpack::pack(file, original);
        }
        // A persisted timestamp ahead of wall time models a clock rollback.
        MetadataStore store(root_ / "rollback");
        store.update(alice_, {{"name", "after rollback"}});
        CPPUNIT_ASSERT_EQUAL(future + 1, store.state().entries.at("name").timestampMs);
        CPPUNIT_ASSERT(!store.merge(original).changed);
        MetadataStore reload(root_ / "rollback");
        reload.update(alice_, {{"name", "after reload"}});
        CPPUNIT_ASSERT_EQUAL(future + 2, reload.state().entries.at("name").timestampMs);

        const auto before = toMillisecondsSinceEpoch(nowMs());
        reload.update(alice_, {{"other", "independent"}});
        const auto after = toMillisecondsSinceEpoch(nowMs());
        const auto stamp = reload.state().entries.at("other").timestampMs;
        CPPUNIT_ASSERT(stamp >= before);
        CPPUNIT_ASSERT(stamp <= after);
    }

    void testPatchAndReload()
    {
        MetadataStore store(root_ / "a");
        CPPUNIT_ASSERT(store.values().empty());
        auto first = store.update(alice_, {{"name", "Président"}, {"empty", ""}});
        CPPUNIT_ASSERT(first.changed && first.valuesChanged);
        auto before = store.state();
        auto noop = store.update(alice_, {{"name", "Président"}});
        CPPUNIT_ASSERT(!noop.changed && !noop.valuesChanged);
        CPPUNIT_ASSERT(store.state() == before);
        store.update(alice_, {{"other", "second"}});
        MetadataStore reload(root_ / "a");
        CPPUNIT_ASSERT(reload.values() == store.values());
        CPPUNIT_ASSERT(reload.state() == store.state());
        reload.update(bob_, {{"name", "renamed"}});
        CPPUNIT_ASSERT(reload.state().entries.at("name").timestampMs > before.entries.at("name").timestampMs);
        CPPUNIT_ASSERT_EQUAL(std::string("second"), reload.values().at("other"));
    }

    void testDeterministicConcurrentMerge()
    {
        MetadataStore a(root_ / "a"), b(root_ / "b"), c(root_ / "c");
        Metadata as, bs;
        as.entries = {{"same", {1000, alice_, "Alice"}}, {"only-a", {1000, alice_, "1"}}};
        bs.entries = {{"same", {1000, bob_, "Bob"}}, {"only-b", {1000, bob_, "1"}}};
        a.merge(as);
        b.merge(bs);
        a.merge(bs);
        b.merge(as);
        CPPUNIT_ASSERT(a.state() == b.state());
        CPPUNIT_ASSERT_EQUAL(std::string("Bob"), a.values().at("same"));
        CPPUNIT_ASSERT_EQUAL(size_t(3), a.values().size());
        CPPUNIT_ASSERT(!a.merge(b.state()).changed);
        CPPUNIT_ASSERT(!b.merge(a.state()).changed);
        // A previously offline third replica receives changes learned by an intermediary.
        c.merge(a.state());
        CPPUNIT_ASSERT(c.state() == b.state());
        c.update(alice_, {{"same", "causally newer"}});
        b.merge(c.state());
        a.merge(b.state());
        CPPUNIT_ASSERT(a.state() == c.state());
    }

    void testRemovalMarkersSurviveStalePeer()
    {
        MetadataStore a(root_ / "a"), b(root_ / "b");
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
        MetadataStore reload(root_ / "b");
        CPPUNIT_ASSERT(reload.state() == a.state());
    }

    void testStampOnlyMerge()
    {
        MetadataStore a(root_ / "a"), b(root_ / "b");
        a.update(alice_, {{"name", "same"}});
        b.update(bob_, {{"name", "same"}});
        auto change = a.merge(b.state());
        CPPUNIT_ASSERT(change.changed);
        CPPUNIT_ASSERT(!change.valuesChanged);
        CPPUNIT_ASSERT(a.state() == b.state());
        MetadataStore reload(root_ / "a");
        CPPUNIT_ASSERT(reload.state() == b.state());
    }

    void testInvalidInputIsAtomic()
    {
        MetadataStore a(root_ / "a");
        a.update(alice_, {{"name", "original"}});
        auto before = a.state();
        CPPUNIT_ASSERT_THROW(a.update(alice_, {{"", "bad"}, {"name", "changed"}}), std::invalid_argument);
        CPPUNIT_ASSERT_THROW(a.update(alice_, {{"name", std::string(65537, 'x')}}), std::invalid_argument);
        CPPUNIT_ASSERT_THROW(a.update(alice_, {{std::string(1025, 'x'), "v"}}), std::invalid_argument);
        CPPUNIT_ASSERT_THROW(a.update(alice_, {{"name", "\xc0\xaf"}}), std::invalid_argument);
        CPPUNIT_ASSERT_THROW(a.update(alice_, {{"name", std::string("x\0y", 3)}}), std::invalid_argument);
        CPPUNIT_ASSERT_THROW(a.update("not-a-replica", {{"name", "value"}}), std::invalid_argument);
        auto conflicting = before;
        conflicting.entries.at("name").value = "forged";
        CPPUNIT_ASSERT_THROW(a.merge(conflicting), std::invalid_argument);
        auto invalid = before;
        invalid.entries.at("name").timestampMs = 0;
        CPPUNIT_ASSERT_THROW(a.merge(invalid), std::invalid_argument);
        invalid.entries.at("name").timestampMs = -1;
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
        MetadataStore corrupt(root_ / "corrupt");
        CPPUNIT_ASSERT_THROW(corrupt.values(), std::exception);
        CPPUNIT_ASSERT_THROW(corrupt.update(alice_, {{"name", "lost"}}), std::exception);
        CPPUNIT_ASSERT_EQUAL(uintmax_t(7), std::filesystem::file_size(root_ / "corrupt"));
        CPPUNIT_ASSERT_THROW(MetadataStore::decode(std::string_view("\x80", 1)), std::exception);
        msgpack::sbuffer packed;
        msgpack::pack(packed, Metadata {});
        CPPUNIT_ASSERT(MetadataStore::decode({packed.data(), packed.size()}).entries.empty());
        packed.write("\x00", 1);
        CPPUNIT_ASSERT_THROW(MetadataStore::decode({packed.data(), packed.size()}), std::exception);
        MetadataStore missing(root_ / "missing" / "state");
        CPPUNIT_ASSERT_THROW(missing.update(alice_, {{"name", "lost"}}), std::exception);
        CPPUNIT_ASSERT(missing.values().empty());
        MetadataStore a(root_ / "a");
        a.update(alice_, {{"name", "saved"}});
        auto before = a.state();
        std::filesystem::create_directory(root_ / "a.new");
        CPPUNIT_ASSERT_THROW(a.update(alice_, {{"name", "not saved"}}), std::exception);
        CPPUNIT_ASSERT(a.state() == before);
        MetadataStore reload(root_ / "a");
        CPPUNIT_ASSERT(reload.state() == before);

        MetadataStore blocked(root_ / "blocked");
        CPPUNIT_ASSERT(blocked.values().empty());
        std::filesystem::create_directory(root_ / "blocked");
        CPPUNIT_ASSERT_THROW(blocked.update(alice_, {{"name", "not saved"}}), std::filesystem::filesystem_error);
        CPPUNIT_ASSERT(std::filesystem::is_directory(root_ / "blocked"));
        CPPUNIT_ASSERT(!std::filesystem::exists(root_ / "blocked.new"));
        std::filesystem::remove(root_ / "blocked");
        CPPUNIT_ASSERT(blocked.values().empty());
        CPPUNIT_ASSERT(blocked.update(alice_, {{"name", "retried"}}).changed);
        MetadataStore retry(root_ / "blocked");
        CPPUNIT_ASSERT(retry.state() == blocked.state());
    }

    void testConcurrentUpdates()
    {
        MetadataStore a(root_ / "a");
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
        MetadataStore reload(root_ / "a");
        CPPUNIT_ASSERT(reload.state() == a.state());
    }

    void testFutureTimestampAndOverflow()
    {
        MetadataStore store(root_ / "overflow");
        Metadata remote;
        const auto maximum = std::numeric_limits<int64_t>::max();
        remote.entries["name"] = {maximum - 1, bob_, "received"};
        msgpack::sbuffer packed;
        msgpack::pack(packed, remote);
        auto decoded = MetadataStore::decode({packed.data(), packed.size()});
        CPPUNIT_ASSERT(store.merge(decoded).changed);
        store.update(alice_, {{"name", "at maximum"}});
        CPPUNIT_ASSERT_EQUAL(maximum, store.state().entries.at("name").timestampMs);
        const auto before = store.state();
        CPPUNIT_ASSERT_THROW(store.update(alice_, {{"a", "not committed"}, {"name", "overflow"}}), std::overflow_error);
        CPPUNIT_ASSERT(store.state() == before);
        MetadataStore reloaded(root_ / "overflow");
        CPPUNIT_ASSERT(reloaded.state() == before);
        CPPUNIT_ASSERT(!reloaded.update(alice_, {{"name", "at maximum"}}).changed);
        CPPUNIT_ASSERT(reloaded.update(alice_, {{"other", "still writable"}}).changed);
    }

    CPPUNIT_TEST_SUITE(MetadataTest);
    CPPUNIT_TEST(testWallTimeTimestamps);
    CPPUNIT_TEST(testClockRollbackAndKeyIndependence);
    CPPUNIT_TEST(testPatchAndReload);
    CPPUNIT_TEST(testDeterministicConcurrentMerge);
    CPPUNIT_TEST(testRemovalMarkersSurviveStalePeer);
    CPPUNIT_TEST(testStampOnlyMerge);
    CPPUNIT_TEST(testInvalidInputIsAtomic);
    CPPUNIT_TEST(testCorruptionAndWriteFailure);
    CPPUNIT_TEST(testConcurrentUpdates);
    CPPUNIT_TEST(testFutureTimestampAndOverflow);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MetadataTest, MetadataTest::name());
} // namespace jami::test

CORE_TEST_RUNNER(jami::test::MetadataTest::name())
