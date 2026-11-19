/*
 * Copyright (C) 2026 Savoir-faire Linux Inc.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "metadata.h"
#include "metadata_store.h"
#include "jamidht/timestamp.h"
#include "../../test_runner.h"

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <fstream>
#include <limits>
#include <thread>

namespace jami::test {
namespace {

std::vector<uint8_t>
bytes(std::string_view text)
{
    return {text.begin(), text.end()};
}

} // namespace

class MetadataTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "metadata"; }

private:
    const std::string alice_ = std::string(64, 'a');
    const std::string bob_ = std::string(64, 'b');

    void testWallTimeTimestamps()
    {
        Metadata state;
        const auto before = toMillisecondsSinceEpoch(nowMs());
        CPPUNIT_ASSERT(state.update(alice_, {{"name", bytes("first")}, {"other", bytes("value")}}));
        const auto after = toMillisecondsSinceEpoch(nowMs());
        for (const auto& [key, entry] : state.entries) {
            CPPUNIT_ASSERT(entry.timestampMs >= before);
            CPPUNIT_ASSERT(entry.timestampMs <= after);
        }
    }

    void testClockRollbackAndKeyIndependence()
    {
        Metadata state;
        const auto future = toMillisecondsSinceEpoch(nowMs()) + 3600000;
        state.entries["name"] = {future, alice_, bytes("before rollback")};
        const auto original = state;
        CPPUNIT_ASSERT(state.update(alice_, {{"name", bytes("after rollback")}}));
        CPPUNIT_ASSERT_EQUAL(future + 1, state.entries.at("name").timestampMs);
        const auto stale = state.merge(original);
        CPPUNIT_ASSERT(stale && !stale->changed);
        const auto before = toMillisecondsSinceEpoch(nowMs());
        CPPUNIT_ASSERT(state.update(alice_, {{"other", bytes("independent")}}));
        const auto after = toMillisecondsSinceEpoch(nowMs());
        const auto stamp = state.entries.at("other").timestampMs;
        CPPUNIT_ASSERT(stamp >= before);
        CPPUNIT_ASSERT(stamp <= after);
    }

    void testPatch()
    {
        Metadata state;
        CPPUNIT_ASSERT(state.values().empty());
        const auto first = state.update(alice_, {{"name", {0x80, 0, 0xff}}, {"empty", {}}});
        CPPUNIT_ASSERT(first && first->changed && first->snapshot);
        CPPUNIT_ASSERT(*first->snapshot == state.values());
        const auto before = state;
        const auto noop = state.update(alice_, {{"name", {0x80, 0, 0xff}}});
        CPPUNIT_ASSERT(noop && !noop->changed && !noop->snapshot);
        CPPUNIT_ASSERT(state == before);
        CPPUNIT_ASSERT(state.update(alice_, {{"other", bytes("second")}}));
        CPPUNIT_ASSERT(state.update(bob_, {{"name", bytes("renamed")}}));
        CPPUNIT_ASSERT(state.entries.at("name").timestampMs > before.entries.at("name").timestampMs);
        CPPUNIT_ASSERT(state.values().at("other") == bytes("second"));
        CPPUNIT_ASSERT(state.values().at("empty").empty());
    }

    void testDeterministicConcurrentMerge()
    {
        Metadata a, b, c;
        a.entries = {{"same", {1000, alice_, bytes("Alice")}}, {"only-a", {1000, alice_, {'1'}}}};
        b.entries = {{"same", {1000, bob_, bytes("Bob")}}, {"only-b", {1000, bob_, {'1'}}}};
        const auto as = a;
        const auto bs = b;
        CPPUNIT_ASSERT(a.merge(bs));
        CPPUNIT_ASSERT(b.merge(as));
        CPPUNIT_ASSERT(a == b);
        CPPUNIT_ASSERT(a.values().at("same") == bytes("Bob"));
        CPPUNIT_ASSERT_EQUAL(size_t(3), a.values().size());
        const auto noopA = a.merge(b);
        const auto noopB = b.merge(a);
        CPPUNIT_ASSERT(noopA && !noopA->changed);
        CPPUNIT_ASSERT(noopB && !noopB->changed);
        // An offline replica receives changes learned by an intermediary.
        CPPUNIT_ASSERT(c.merge(a));
        CPPUNIT_ASSERT(c == b);
        CPPUNIT_ASSERT(c.update(alice_, {{"same", bytes("causally newer")}}));
        CPPUNIT_ASSERT(b.merge(c));
        CPPUNIT_ASSERT(a.merge(b));
        CPPUNIT_ASSERT(a == c);
    }

    void testRemovalMarkersSurviveStalePeer()
    {
        Metadata a, b;
        const std::string member = "jami.channels.v1/stable/members/uri";
        const std::string deleted = "jami.channels.v1/stable/deleted";
        CPPUNIT_ASSERT(a.update(alice_, {{member, {'1'}}, {"unrelated", bytes("kept")}}));
        CPPUNIT_ASSERT(b.merge(a));
        const auto stale = b;
        CPPUNIT_ASSERT(a.update(alice_, {{member, {'0'}}, {deleted, {'1'}}}));
        CPPUNIT_ASSERT(b.merge(a));
        const auto noop = a.merge(stale);
        CPPUNIT_ASSERT(noop && !noop->changed);
        CPPUNIT_ASSERT(b.values().at(member) == bytes("0"));
        CPPUNIT_ASSERT(b.values().at(deleted) == bytes("1"));
        CPPUNIT_ASSERT(b.values().at("unrelated") == bytes("kept"));
        CPPUNIT_ASSERT(a == b);
    }

    void testStampOnlyMerge()
    {
        Metadata a, b;
        a.entries["name"] = {1000, alice_, bytes("same")};
        b.entries["name"] = {1000, bob_, bytes("same")};
        const auto change = a.merge(b);
        CPPUNIT_ASSERT(change && change->changed && !change->snapshot);
        CPPUNIT_ASSERT(a == b);
    }

    void testMixedMergeSnapshot()
    {
        Metadata state;
        state.entries = {{"name", {1000, alice_, bytes("same")}}, {"kept", {1000, alice_, bytes("unchanged")}}};
        Metadata incoming;
        incoming.entries = {{"name", {2000, bob_, bytes("same")}}, {"empty", {1000, bob_, {}}}};
        const auto change = state.merge(incoming);
        CPPUNIT_ASSERT(change && change->changed && change->snapshot);
        CPPUNIT_ASSERT(*change->snapshot == state.values());
        CPPUNIT_ASSERT(change->snapshot->at("kept") == bytes("unchanged"));
        CPPUNIT_ASSERT(change->snapshot->at("empty").empty());
        incoming.entries.at("name") = {1500, alice_, bytes("losing")};
        const auto noop = state.merge(incoming);
        CPPUNIT_ASSERT(noop && !noop->changed && !noop->snapshot);
    }

    void testInvalidInputIsAtomic()
    {
        Metadata state;
        CPPUNIT_ASSERT(state.update(alice_, {{"name", bytes("original")}}));
        const auto before = state;
        CPPUNIT_ASSERT(!state.update(alice_, {{"", bytes("bad")}, {"name", bytes("changed")}}));
        CPPUNIT_ASSERT(!state.update(alice_, {{"name", std::vector<uint8_t>(65537, 0xff)}}));
        CPPUNIT_ASSERT(!state.update(alice_, {{std::string(1025, 'x'), {1}}}));
        CPPUNIT_ASSERT(!state.update(alice_, {{"\xc0\xaf", {1}}}));
        CPPUNIT_ASSERT(!state.update(alice_, {{std::string("x\0y", 3), {1}}}));
        CPPUNIT_ASSERT(!state.update("not-a-replica", {{"name", bytes("value")}}));
        auto invalid = before;
        invalid.entries.at("name").timestampMs = 0;
        CPPUNIT_ASSERT(!state.merge(invalid));
        invalid.entries.at("name").timestampMs = -1;
        CPPUNIT_ASSERT(!state.merge(invalid));
        CPPUNIT_ASSERT(state == before);
        Metadata::Values oversized;
        for (unsigned i = 0; i < 65; ++i)
            oversized[std::to_string(i)] = std::vector<uint8_t>(65536, 0xff);
        CPPUNIT_ASSERT(!state.update(alice_, oversized));
        CPPUNIT_ASSERT(state == before);
    }

    void testMergeFailureIsAtomic()
    {
        Metadata state;
        const auto updated = state.update(alice_, {{"name", bytes("original")}});
        CPPUNIT_ASSERT(updated && updated->changed && updated->snapshot);
        const auto before = state;
        auto remote = state;
        remote.entries["a"] = {1000, alice_, bytes("must not be committed")};
        remote.entries.at("name").value = bytes("conflicting");
        CPPUNIT_ASSERT(!state.merge(remote));
        CPPUNIT_ASSERT(state == before);
        CPPUNIT_ASSERT(state.values().at("name") == bytes("original"));
    }

    void testDecodeInvalidRecords()
    {
        CPPUNIT_ASSERT(!Metadata::decode(std::string_view("\x80", 1)));
        CPPUNIT_ASSERT(!Metadata::decode(std::string_view("\xc1", 1)));
        const std::map<std::string, std::map<std::string, std::map<std::string, std::string>>> wrongType {
            {"entries", {{"key", {{"timestampMs", "not-an-integer"}, {"writer", alice_}, {"value", "bad"}}}}}};
        msgpack::sbuffer invalid;
        msgpack::pack(invalid, wrongType);
        CPPUNIT_ASSERT(!Metadata::decode({invalid.data(), invalid.size()}));
        msgpack::sbuffer packed;
        msgpack::pack(packed, Metadata {});
        const auto decoded = Metadata::decode({packed.data(), packed.size()});
        CPPUNIT_ASSERT(decoded && decoded->entries.empty());
        packed.write("\x00", 1);
        CPPUNIT_ASSERT(!Metadata::decode({packed.data(), packed.size()}));
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
            const auto decoded = Metadata::decode({buffer.data(), buffer.size()});
            CPPUNIT_ASSERT_EQUAL(withValue, decoded.has_value());
            if (decoded) {
                CPPUNIT_ASSERT_EQUAL(alice_, decoded->entries.at("key").writer);
                CPPUNIT_ASSERT(decoded->entries.at("key").value.empty());
            }
        }
    }

    void testBinaryEncodingAndLegacyDecoding()
    {
        Metadata state;
        state.entries["key"] = {1000, alice_, {0, 0x80, 0xff}};

        msgpack::sbuffer packed;
        msgpack::pack(packed, state);
        const auto object = msgpack::unpack(packed.data(), packed.size());
        using ObjectMap = std::map<std::string, msgpack::object>;
        const auto entries = object.get().as<ObjectMap>().at("entries").as<ObjectMap>();
        const auto fields = entries.at("key").as<ObjectMap>();
        CPPUNIT_ASSERT(fields.at("value").type == msgpack::type::BIN);
        const auto decoded = Metadata::decode({packed.data(), packed.size()});
        CPPUNIT_ASSERT(decoded && *decoded == state);

        // Legacy devices and files encoded values as MessagePack strings.
        msgpack::sbuffer legacy;
        msgpack::packer<msgpack::sbuffer> packer(legacy);
        packer.pack_map(1).pack(std::string("entries"));
        packer.pack_map(1).pack(std::string("key"));
        packer.pack_map(3)
            .pack(std::string("timestampMs"))
            .pack(int64_t(1000))
            .pack(std::string("writer"))
            .pack(alice_)
            .pack(std::string("value"))
            .pack(std::string("\0\x80\xff", 3));
        const auto legacyDecoded = Metadata::decode({legacy.data(), legacy.size()});
        CPPUNIT_ASSERT(legacyDecoded && *legacyDecoded == state);
    }

    void testFutureTimestampAndOverflow()
    {
        Metadata state, remote;
        const auto maximum = std::numeric_limits<int64_t>::max();
        remote.entries["name"] = {maximum - 1, bob_, bytes("received")};
        msgpack::sbuffer packed;
        msgpack::pack(packed, remote);
        const auto decoded = Metadata::decode({packed.data(), packed.size()});
        CPPUNIT_ASSERT(decoded);
        const auto merged = state.merge(*decoded);
        CPPUNIT_ASSERT(merged && merged->changed);
        CPPUNIT_ASSERT(state.update(alice_, {{"name", bytes("at maximum")}}));
        const auto before = state;
        CPPUNIT_ASSERT_EQUAL(maximum, before.entries.at("name").timestampMs);
        CPPUNIT_ASSERT(!state.update(alice_, {{"a", bytes("not committed")}, {"name", bytes("overflow")}}));
        CPPUNIT_ASSERT(state == before);
        const auto noop = state.update(alice_, {{"name", bytes("at maximum")}});
        CPPUNIT_ASSERT(noop && !noop->changed && !noop->snapshot);
        const auto changed = state.update(alice_, {{"other", bytes("still writable")}});
        CPPUNIT_ASSERT(changed && changed->changed);
    }

    CPPUNIT_TEST_SUITE(MetadataTest);
    CPPUNIT_TEST(testWallTimeTimestamps);
    CPPUNIT_TEST(testClockRollbackAndKeyIndependence);
    CPPUNIT_TEST(testPatch);
    CPPUNIT_TEST(testDeterministicConcurrentMerge);
    CPPUNIT_TEST(testRemovalMarkersSurviveStalePeer);
    CPPUNIT_TEST(testStampOnlyMerge);
    CPPUNIT_TEST(testMixedMergeSnapshot);
    CPPUNIT_TEST(testInvalidInputIsAtomic);
    CPPUNIT_TEST(testMergeFailureIsAtomic);
    CPPUNIT_TEST(testDecodeInvalidRecords);
    CPPUNIT_TEST(testDecodeRequiredFields);
    CPPUNIT_TEST(testBinaryEncodingAndLegacyDecoding);
    CPPUNIT_TEST(testFutureTimestampAndOverflow);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MetadataTest, MetadataTest::name());

class MetadataStoreTest : public CppUnit::TestFixture
{
public:
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
        Metadata::Values values() const
        {
            auto result = MetadataStore::values();
            CPPUNIT_ASSERT(result);
            return std::move(*result);
        }
        Metadata::Change update(const std::string& writer, const Metadata::Values& updates)
        {
            auto result = MetadataStore::update(writer, updates);
            CPPUNIT_ASSERT(result);
            return std::move(*result);
        }
        Metadata::Change merge(const Metadata& remote)
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

    void testPatchAndReload()
    {
        Store store(root_ / "a");
        CPPUNIT_ASSERT(store.values().empty());
        const auto first = store.update(alice_, {{"name", {0, 0xff, 0x80}}, {"empty", {}}});
        CPPUNIT_ASSERT(first.changed && first.snapshot);
        CPPUNIT_ASSERT(*first.snapshot == store.values());
        const auto before = store.state();
        const auto noop = store.update(alice_, {{"name", {0, 0xff, 0x80}}});
        CPPUNIT_ASSERT(!noop.changed && !noop.snapshot);
        CPPUNIT_ASSERT(store.state() == before);
        store.update(alice_, {{"other", bytes("second")}});
        Store reload(root_ / "a");
        CPPUNIT_ASSERT(reload.values() == store.values());
        CPPUNIT_ASSERT(reload.state() == store.state());
        reload.update(bob_, {{"name", bytes("renamed")}});
        CPPUNIT_ASSERT(reload.state().entries.at("name").timestampMs > before.entries.at("name").timestampMs);
        CPPUNIT_ASSERT(reload.values().at("other") == bytes("second"));
        Metadata remote;
        const auto future = toMillisecondsSinceEpoch(nowMs()) + 3600000;
        remote.entries["name"] = {future, bob_, bytes("same after merge")};
        const auto merged = reload.merge(remote);
        CPPUNIT_ASSERT(merged.changed && merged.snapshot);
        ++remote.entries.at("name").timestampMs;
        const auto stampOnly = reload.merge(remote);
        CPPUNIT_ASSERT(stampOnly.changed && !stampOnly.snapshot);
        Store mergedReload(root_ / "a");
        CPPUNIT_ASSERT(mergedReload.state() == reload.state());
        mergedReload.update(alice_, {{"name", bytes("after rollback")}});
        CPPUNIT_ASSERT_EQUAL(future + 2, mergedReload.state().entries.at("name").timestampMs);
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
        CPPUNIT_ASSERT(!corrupt.update(alice_, {{"name", bytes("lost")}}));
        CPPUNIT_ASSERT_EQUAL(uintmax_t(7), std::filesystem::file_size(root_ / "corrupt"));
        MetadataStore missing(root_ / "missing" / "state");
        CPPUNIT_ASSERT(!missing.update(alice_, {{"name", bytes("lost")}}));
        const auto missingValues = missing.values();
        CPPUNIT_ASSERT(missingValues && missingValues->empty());
        MetadataStore a(root_ / "a");
        CPPUNIT_ASSERT(a.update(alice_, {{"name", bytes("saved")}}));
        const auto before = a.state();
        CPPUNIT_ASSERT(before);
        std::filesystem::create_directory(root_ / "a.new");
        CPPUNIT_ASSERT(!a.update(alice_, {{"name", bytes("not saved")}}));
        CPPUNIT_ASSERT(std::filesystem::create_directory(root_ / "a.new"));
        Metadata remote;
        remote.entries["other"] = {1000, bob_, bytes("not saved")};
        CPPUNIT_ASSERT(!a.merge(remote));
        CPPUNIT_ASSERT(a.state() == before);
        MetadataStore reload(root_ / "a");
        CPPUNIT_ASSERT(reload.state() == before);

        MetadataStore blocked(root_ / "blocked");
        const auto emptyValues = blocked.values();
        CPPUNIT_ASSERT(emptyValues && emptyValues->empty());
        std::filesystem::create_directory(root_ / "blocked");
        CPPUNIT_ASSERT(!blocked.update(alice_, {{"name", bytes("not saved")}}));
        CPPUNIT_ASSERT(std::filesystem::is_directory(root_ / "blocked"));
        CPPUNIT_ASSERT(!std::filesystem::exists(root_ / "blocked.new"));
        std::filesystem::remove(root_ / "blocked");
        CPPUNIT_ASSERT(blocked.values() == emptyValues);
        const auto retried = blocked.update(alice_, {{"name", bytes("retried")}});
        CPPUNIT_ASSERT(retried && retried->changed);
        MetadataStore retry(root_ / "blocked");
        CPPUNIT_ASSERT(retry.state() == blocked.state());
    }

    void testConcurrentUpdates()
    {
        Store a(root_ / "a");
        std::thread first([&] {
            for (int i = 0; i < 20; ++i)
                a.update(alice_, {{"a" + std::to_string(i), {'1'}}});
        });
        std::thread second([&] {
            for (int i = 0; i < 20; ++i)
                a.update(bob_, {{"b" + std::to_string(i), {'1'}}});
        });
        first.join();
        second.join();
        CPPUNIT_ASSERT_EQUAL(size_t(40), a.values().size());
        Store reload(root_ / "a");
        CPPUNIT_ASSERT(reload.state() == a.state());
    }

    void testFutureTimestampReload()
    {
        MetadataStore store(root_ / "overflow");
        Metadata remote;
        const auto maximum = std::numeric_limits<int64_t>::max();
        remote.entries["name"] = {maximum - 1, bob_, bytes("received")};
        CPPUNIT_ASSERT(store.merge(remote));
        CPPUNIT_ASSERT(store.update(alice_, {{"name", bytes("at maximum")}}));
        const auto before = store.state();
        CPPUNIT_ASSERT(before);
        CPPUNIT_ASSERT_EQUAL(maximum, before->entries.at("name").timestampMs);
        MetadataStore reloaded(root_ / "overflow");
        CPPUNIT_ASSERT(reloaded.state() == before);
        CPPUNIT_ASSERT(!reloaded.update(alice_, {{"a", bytes("not committed")}, {"name", bytes("overflow")}}));
        CPPUNIT_ASSERT(reloaded.state() == before);
        const auto noop = reloaded.update(alice_, {{"name", bytes("at maximum")}});
        CPPUNIT_ASSERT(noop && !noop->changed && !noop->snapshot);
        const auto changed = reloaded.update(alice_, {{"other", bytes("still writable")}});
        CPPUNIT_ASSERT(changed && changed->changed);
    }

    CPPUNIT_TEST_SUITE(MetadataStoreTest);
    CPPUNIT_TEST(testPatchAndReload);
    CPPUNIT_TEST(testCorruptionAndWriteFailure);
    CPPUNIT_TEST(testConcurrentUpdates);
    CPPUNIT_TEST(testFutureTimestampReload);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MetadataStoreTest, MetadataTest::name());
} // namespace jami::test

CORE_TEST_RUNNER(jami::test::MetadataTest::name())
