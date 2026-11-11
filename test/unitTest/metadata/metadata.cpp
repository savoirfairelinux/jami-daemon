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
    class Store : public MetadataStore
    {
    public:
        using MetadataStore::MetadataStore;

        Metadata state() const
        {
            auto result = MetadataStore::state();
            CPPUNIT_ASSERT(result);
            return std::move(*result);
        }
        std::map<std::string, std::string> values() const
        {
            auto result = MetadataStore::values();
            CPPUNIT_ASSERT(result);
            return std::move(*result);
        }
        Change update(const std::string& writer, const std::map<std::string, std::string>& updates)
        {
            auto result = MetadataStore::update(writer, updates);
            CPPUNIT_ASSERT(result);
            return std::move(*result);
        }
        Change merge(const Metadata& remote)
        {
            auto result = MetadataStore::merge(remote);
            CPPUNIT_ASSERT(result);
            return std::move(*result);
        }
    };

    const std::filesystem::path root_ {"metadata-test-state"};
    bool ownsRoot_ {false};
    const std::string alice_ = std::string(64, 'a');
    const std::string bob_ = std::string(64, 'b');

    void testWallTimeTimestamps()
    {
        Store store(root_ / "timestamps");
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
        Store store(root_ / "rollback");
        store.update(alice_, {{"name", "after rollback"}});
        CPPUNIT_ASSERT_EQUAL(future + 1, store.state().entries.at("name").timestampMs);
        CPPUNIT_ASSERT(!store.merge(original).changed);
        Store reload(root_ / "rollback");
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
        Store store(root_ / "a");
        CPPUNIT_ASSERT(store.values().empty());
        auto first = store.update(alice_, {{"name", "Président"}, {"empty", ""}});
        CPPUNIT_ASSERT(first.changed && first.snapshot);
        CPPUNIT_ASSERT(*first.snapshot == store.values());
        auto before = store.state();
        auto noop = store.update(alice_, {{"name", "Président"}});
        CPPUNIT_ASSERT(!noop.changed && !noop.snapshot);
        CPPUNIT_ASSERT(store.state() == before);
        store.update(alice_, {{"other", "second"}});
        Store reload(root_ / "a");
        CPPUNIT_ASSERT(reload.values() == store.values());
        CPPUNIT_ASSERT(reload.state() == store.state());
        reload.update(bob_, {{"name", "renamed"}});
        CPPUNIT_ASSERT(reload.state().entries.at("name").timestampMs > before.entries.at("name").timestampMs);
        CPPUNIT_ASSERT_EQUAL(std::string("second"), reload.values().at("other"));
    }

    void testDeterministicConcurrentMerge()
    {
        Store a(root_ / "a"), b(root_ / "b"), c(root_ / "c");
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
        Store a(root_ / "a"), b(root_ / "b");
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
        Store reload(root_ / "b");
        CPPUNIT_ASSERT(reload.state() == a.state());
    }

    void testStampOnlyMerge()
    {
        Store a(root_ / "a"), b(root_ / "b");
        a.update(alice_, {{"name", "same"}});
        b.update(bob_, {{"name", "same"}});
        auto change = a.merge(b.state());
        CPPUNIT_ASSERT(change.changed);
        CPPUNIT_ASSERT(!change.snapshot);
        CPPUNIT_ASSERT(a.state() == b.state());
        Store reload(root_ / "a");
        CPPUNIT_ASSERT(reload.state() == b.state());
    }

    void testMixedMergeSnapshot()
    {
        Store store(root_ / "mixed");
        Metadata original;
        original.entries = {{"name", {1000, alice_, "same"}}, {"kept", {1000, alice_, "unchanged"}}};
        store.merge(original);
        Metadata incoming;
        incoming.entries = {{"name", {2000, bob_, "same"}}, {"empty", {1000, bob_, ""}}};
        const auto change = store.merge(incoming);
        CPPUNIT_ASSERT(change.changed && change.snapshot);
        CPPUNIT_ASSERT(*change.snapshot == store.values());
        CPPUNIT_ASSERT_EQUAL(std::string("unchanged"), change.snapshot->at("kept"));
        CPPUNIT_ASSERT(change.snapshot->at("empty").empty());
        incoming.entries.at("name") = {1500, alice_, "losing"};
        const auto noop = store.merge(incoming);
        CPPUNIT_ASSERT(!noop.changed && !noop.snapshot);
    }

    void testInvalidInputIsAtomic()
    {
        MetadataStore a(root_ / "a");
        CPPUNIT_ASSERT(a.update(alice_, {{"name", "original"}}));
        auto before = a.state();
        CPPUNIT_ASSERT(before);
        CPPUNIT_ASSERT(!a.update(alice_, {{"", "bad"}, {"name", "changed"}}));
        CPPUNIT_ASSERT(!a.update(alice_, {{"name", std::string(65537, 'x')}}));
        CPPUNIT_ASSERT(!a.update(alice_, {{std::string(1025, 'x'), "v"}}));
        CPPUNIT_ASSERT(!a.update(alice_, {{"name", "\xc0\xaf"}}));
        CPPUNIT_ASSERT(!a.update(alice_, {{"name", std::string("x\0y", 3)}}));
        CPPUNIT_ASSERT(!a.update("not-a-replica", {{"name", "value"}}));
        auto conflicting = *before;
        conflicting.entries.at("name").value = "forged";
        CPPUNIT_ASSERT(!a.merge(conflicting));
        auto invalid = *before;
        invalid.entries.at("name").timestampMs = 0;
        CPPUNIT_ASSERT(!a.merge(invalid));
        invalid.entries.at("name").timestampMs = -1;
        CPPUNIT_ASSERT(!a.merge(invalid));
        CPPUNIT_ASSERT(a.state() == before);
        std::map<std::string, std::string> oversized;
        for (unsigned i = 0; i < 65; ++i)
            oversized[std::to_string(i)] = std::string(65536, 'x');
        CPPUNIT_ASSERT(!a.update(alice_, oversized));
        CPPUNIT_ASSERT(a.state() == before);
    }

    void testCorruptionAndWriteFailure()
    {
        {
            std::ofstream file(root_ / "corrupt");
            file << "corrupt";
        }
        MetadataStore corrupt(root_ / "corrupt");
        CPPUNIT_ASSERT(!corrupt.state());
        CPPUNIT_ASSERT(!corrupt.values());
        CPPUNIT_ASSERT(!corrupt.update(alice_, {{"name", "lost"}}));
        CPPUNIT_ASSERT_EQUAL(uintmax_t(7), std::filesystem::file_size(root_ / "corrupt"));
        CPPUNIT_ASSERT(!MetadataStore::decode(std::string_view("\x80", 1)));
        CPPUNIT_ASSERT(!MetadataStore::decode(std::string_view("\xc1", 1)));
        const std::map<std::string, std::map<std::string, std::map<std::string, std::string>>> wrongType {
            {"entries", {{"key", {{"timestampMs", "not-an-integer"}, {"writer", alice_}, {"value", "bad"}}}}}};
        msgpack::sbuffer invalid;
        msgpack::pack(invalid, wrongType);
        CPPUNIT_ASSERT(!MetadataStore::decode({invalid.data(), invalid.size()}));
        msgpack::sbuffer packed;
        msgpack::pack(packed, Metadata {});
        const auto decoded = MetadataStore::decode({packed.data(), packed.size()});
        CPPUNIT_ASSERT(decoded);
        CPPUNIT_ASSERT(decoded->entries.empty());
        packed.write("\x00", 1);
        CPPUNIT_ASSERT(!MetadataStore::decode({packed.data(), packed.size()}));
        MetadataStore missing(root_ / "missing" / "state");
        CPPUNIT_ASSERT(!missing.update(alice_, {{"name", "lost"}}));
        const auto missingValues = missing.values();
        CPPUNIT_ASSERT(missingValues && missingValues->empty());
        MetadataStore a(root_ / "a");
        CPPUNIT_ASSERT(a.update(alice_, {{"name", "saved"}}));
        auto before = a.state();
        CPPUNIT_ASSERT(before);
        std::filesystem::create_directory(root_ / "a.new");
        CPPUNIT_ASSERT(!a.update(alice_, {{"name", "not saved"}}));
        CPPUNIT_ASSERT(a.state() == before);
        MetadataStore reload(root_ / "a");
        CPPUNIT_ASSERT(reload.state() == before);

        MetadataStore blocked(root_ / "blocked");
        const auto emptyValues = blocked.values();
        CPPUNIT_ASSERT(emptyValues && emptyValues->empty());
        std::filesystem::create_directory(root_ / "blocked");
        CPPUNIT_ASSERT(!blocked.update(alice_, {{"name", "not saved"}}));
        CPPUNIT_ASSERT(std::filesystem::is_directory(root_ / "blocked"));
        CPPUNIT_ASSERT(!std::filesystem::exists(root_ / "blocked.new"));
        std::filesystem::remove(root_ / "blocked");
        CPPUNIT_ASSERT(blocked.values() == emptyValues);
        const auto retried = blocked.update(alice_, {{"name", "retried"}});
        CPPUNIT_ASSERT(retried && retried->changed);
        MetadataStore retry(root_ / "blocked");
        CPPUNIT_ASSERT(retry.state() == blocked.state());
    }

    void testDecodeRequiredFields()
    {
        for (const bool withValue : {false, true}) {
            msgpack::sbuffer buffer;
            msgpack::packer<msgpack::sbuffer> packer(buffer);
            packer.pack_map(1);
            packer.pack(std::string("entries"));
            packer.pack_map(1);
            packer.pack(std::string("key"));
            packer.pack_map(withValue ? 3 : 2);
            packer.pack(std::string("timestampMs"));
            packer.pack(int64_t(1000));
            packer.pack(std::string("writer"));
            packer.pack_bin(alice_.size());
            packer.pack_bin_body(alice_.data(), alice_.size());
            if (withValue) {
                packer.pack(std::string("value"));
                packer.pack_bin(0);
                packer.pack_bin_body("", 0);
            }
            const auto decoded = MetadataStore::decode({buffer.data(), buffer.size()});
            CPPUNIT_ASSERT_EQUAL(withValue, decoded.has_value());
            if (decoded) {
                CPPUNIT_ASSERT_EQUAL(alice_, decoded->entries.at("key").writer);
                CPPUNIT_ASSERT(decoded->entries.at("key").value.empty());
            }
        }
    }

    void testConcurrentUpdates()
    {
        Store a(root_ / "a");
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
        Store reload(root_ / "a");
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
        const auto decoded = MetadataStore::decode({packed.data(), packed.size()});
        CPPUNIT_ASSERT(decoded);
        const auto merged = store.merge(*decoded);
        CPPUNIT_ASSERT(merged && merged->changed);
        CPPUNIT_ASSERT(store.update(alice_, {{"name", "at maximum"}}));
        const auto before = store.state();
        CPPUNIT_ASSERT(before);
        CPPUNIT_ASSERT_EQUAL(maximum, before->entries.at("name").timestampMs);
        CPPUNIT_ASSERT(!store.update(alice_, {{"a", "not committed"}, {"name", "overflow"}}));
        CPPUNIT_ASSERT(store.state() == before);
        MetadataStore reloaded(root_ / "overflow");
        CPPUNIT_ASSERT(reloaded.state() == before);
        const auto noop = reloaded.update(alice_, {{"name", "at maximum"}});
        CPPUNIT_ASSERT(noop && !noop->changed && !noop->snapshot);
        const auto changed = reloaded.update(alice_, {{"other", "still writable"}});
        CPPUNIT_ASSERT(changed && changed->changed);
    }

    CPPUNIT_TEST_SUITE(MetadataTest);
    CPPUNIT_TEST(testWallTimeTimestamps);
    CPPUNIT_TEST(testClockRollbackAndKeyIndependence);
    CPPUNIT_TEST(testPatchAndReload);
    CPPUNIT_TEST(testDeterministicConcurrentMerge);
    CPPUNIT_TEST(testRemovalMarkersSurviveStalePeer);
    CPPUNIT_TEST(testStampOnlyMerge);
    CPPUNIT_TEST(testMixedMergeSnapshot);
    CPPUNIT_TEST(testInvalidInputIsAtomic);
    CPPUNIT_TEST(testCorruptionAndWriteFailure);
    CPPUNIT_TEST(testDecodeRequiredFields);
    CPPUNIT_TEST(testConcurrentUpdates);
    CPPUNIT_TEST(testFutureTimestampAndOverflow);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MetadataTest, MetadataTest::name());
} // namespace jami::test

CORE_TEST_RUNNER(jami::test::MetadataTest::name())
