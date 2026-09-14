/*
 * Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "../../test_runner.h"

#include "jamidht/conversation.h"
#include "jamidht/conversation_module.h"
#include "jamidht/sync_msg_reader.h"

#include <dhtnet/channel_utils.h>
#include <msgpack.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace jami {
namespace test {

// Mimics the on-wire/on-disk layout produced by daemons released before the
// millisecond migration (time_t seconds, MSGPACK_DEFINE_MAP). Used to verify
// both directions of backward compatibility.
struct LegacyConvInfo
{
    std::string id {};
    time_t created {0};
    time_t removed {0};
    time_t erased {0};
    std::set<std::string> members;
    std::string lastDisplayed {};
    ConversationMode mode {0};

    MSGPACK_DEFINE_MAP(id, created, removed, erased, members, lastDisplayed, mode)
};

struct LegacyConversationRequest
{
    std::string conversationId;
    std::string from;
    std::map<std::string, std::string> metadatas;
    time_t received {0};
    time_t declined {0};

    MSGPACK_DEFINE_MAP(from, conversationId, metadatas, received, declined)
};

struct LegacySyncMsg
{
    DeviceSync ds;
    std::map<std::string, ConvInfo> c;
    std::map<std::string, ConversationRequest> cr;
    std::map<std::string, std::map<std::string, std::string>> p;
    std::map<std::string, std::map<std::string, std::string>> ld;
    std::map<std::string, std::map<std::string, std::map<std::string, std::string>>> ms;
    MSGPACK_DEFINE(ds, c, cr, p, ld, ms)
};

class ConvInfoSerializationTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "conv_info_serialization"; }

private:
    // Data written by an old daemon (seconds) is read as seconds * 1000
    void testConvInfoLegacyToNew();
    // Data written by a new daemon is readable by an old daemon (seconds keys)
    void testConvInfoNewToLegacy();
    // Millisecond precision survives a msgpack round-trip between new daemons
    void testConvInfoMsgpackRoundtrip();
    // JSON: ms keys preferred, legacy seconds-only JSON still readable
    void testConvInfoJson();
    // Same checks for ConversationRequest
    void testRequestLegacyToNew();
    void testRequestNewToLegacy();
    void testRequestMsgpackRoundtrip();
    // Client-facing map keeps exposing seconds
    void testRequestToMapStaysSeconds();
    // isRemoved() distinguishes events within the same second
    void testIsRemovedMsResolution();
    void testAccountMetadataSyncCompatibility();
    void testAccountMetadataSyncFrameBoundaries();
    void testAccountMetadataSyncLargeStream();
    void testAccountMetadataSyncMalformedStream();
    void testSyncRejectsOversizedMetadataHeaders();
    void testSyncRejectsOversizedMetadataStrings();
    void testSyncRejectsMetadataBudgetBeforeDelivery();
    void testSyncBoundsIncompleteReassembly();
    void testSyncBoundsContainerHeadersAndDepth();
    void testSyncBoundsAggregateObjectCount();
    void testSyncAcceptsCoalescedObjectsBeyondBufferLimit();
    void testSyncStopsAfterApplicationError();
    void testSyncResetsMetadataBudgetBetweenCoalescedMessages();
    void testSyncPropagatesApplicationExceptionAndStops();
    void testSyncRejectsOversizedWriter();

    CPPUNIT_TEST_SUITE(ConvInfoSerializationTest);
    CPPUNIT_TEST(testConvInfoLegacyToNew);
    CPPUNIT_TEST(testConvInfoNewToLegacy);
    CPPUNIT_TEST(testConvInfoMsgpackRoundtrip);
    CPPUNIT_TEST(testConvInfoJson);
    CPPUNIT_TEST(testRequestLegacyToNew);
    CPPUNIT_TEST(testRequestNewToLegacy);
    CPPUNIT_TEST(testRequestMsgpackRoundtrip);
    CPPUNIT_TEST(testRequestToMapStaysSeconds);
    CPPUNIT_TEST(testIsRemovedMsResolution);
    CPPUNIT_TEST(testAccountMetadataSyncCompatibility);
    CPPUNIT_TEST(testAccountMetadataSyncFrameBoundaries);
    CPPUNIT_TEST(testAccountMetadataSyncLargeStream);
    CPPUNIT_TEST(testAccountMetadataSyncMalformedStream);
    CPPUNIT_TEST(testSyncRejectsOversizedMetadataHeaders);
    CPPUNIT_TEST(testSyncRejectsOversizedMetadataStrings);
    CPPUNIT_TEST(testSyncRejectsMetadataBudgetBeforeDelivery);
    CPPUNIT_TEST(testSyncBoundsIncompleteReassembly);
    CPPUNIT_TEST(testSyncBoundsContainerHeadersAndDepth);
    CPPUNIT_TEST(testSyncBoundsAggregateObjectCount);
    CPPUNIT_TEST(testSyncAcceptsCoalescedObjectsBeyondBufferLimit);
    CPPUNIT_TEST(testSyncStopsAfterApplicationError);
    CPPUNIT_TEST(testSyncResetsMetadataBudgetBetweenCoalescedMessages);
    CPPUNIT_TEST(testSyncPropagatesApplicationExceptionAndStops);
    CPPUNIT_TEST(testSyncRejectsOversizedWriter);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ConvInfoSerializationTest, ConvInfoSerializationTest::name());

template<typename Out, typename In>
static Out
repack(const In& in)
{
    msgpack::sbuffer buffer;
    msgpack::pack(buffer, in);
    msgpack::object_handle oh = msgpack::unpack(buffer.data(), buffer.size());
    Out out;
    oh.get().convert(out);
    return out;
}

void
ConvInfoSerializationTest::testAccountMetadataSyncCompatibility()
{
    LegacySyncMsg legacy;
    legacy.p["conversation"]["color"] = "blue";
    auto current = repack<SyncMsg>(legacy);
    CPPUNIT_ASSERT(current.p == legacy.p);
    CPPUNIT_ASSERT(current.am.entries.empty());
    CPPUNIT_ASSERT(!current.affectsList());
    current.am.clock = 42;
    current.am.entries["jami.channels.v1/id/deleted"] = {42, std::string(64, 'a'), "1"};
    CPPUNIT_ASSERT(current.affectsList());
    auto oldReader = repack<LegacySyncMsg>(current);
    CPPUNIT_ASSERT(oldReader.p == legacy.p);
    auto newReader = repack<SyncMsg>(current);
    CPPUNIT_ASSERT(newReader.am == current.am);
    CPPUNIT_ASSERT(newReader.p == legacy.p);
}

void
ConvInfoSerializationTest::testAccountMetadataSyncFrameBoundaries()
{
    for (size_t size : {size_t(UINT16_MAX) - 1, size_t(UINT16_MAX), size_t(UINT16_MAX) + 1}) {
        SyncMsg sent;
        sent.am.clock = 1;
        auto& entry = sent.am.entries["jami.channels.v1/id/content"];
        entry = {1, std::string(64, 'a'), std::string(size - 512, 'v')};
        msgpack::sbuffer buffer(UINT16_MAX);
        msgpack::pack(buffer, sent);
        CPPUNIT_ASSERT(buffer.size() < size);
        entry.value.append(size - buffer.size(), 'v');
        buffer.clear();
        msgpack::pack(buffer, sent);
        CPPUNIT_ASSERT_EQUAL(size, buffer.size());
        CPPUNIT_ASSERT_NO_THROW(AccountMetadataStore::validate(sent.am));

        size_t received = 0;
        auto reader = buildSyncMsgReader([&](SyncMsg&& msg) {
            CPPUNIT_ASSERT(msg.am == sent.am);
            ++received;
            return std::error_code {};
        });
        auto bytes = reinterpret_cast<const uint8_t*>(buffer.data());
        // A partial object must not be delivered, including when it spans frames.
        CPPUNIT_ASSERT_EQUAL(ssize_t(size - 1), reader(bytes, size - 1));
        CPPUNIT_ASSERT_EQUAL(size_t(0), received);
        CPPUNIT_ASSERT_EQUAL(ssize_t(1), reader(bytes + size - 1, 1));
        CPPUNIT_ASSERT_EQUAL(size_t(1), received);
    }
}

void
ConvInfoSerializationTest::testAccountMetadataSyncLargeStream()
{
    SyncMsg sent;
    size_t remaining = AccountMetadataStore::MAX_STATE_BYTES;
    while (remaining) {
        auto key = "jami.channels.v1/" + std::to_string(sent.am.entries.size()) + "/content";
        // Match the store's conservative serialized-size accounting.
        const auto overhead = key.size() + 64 + 64;
        CPPUNIT_ASSERT(remaining >= overhead);
        const auto length = std::min(AccountMetadataStore::MAX_VALUE_BYTES, remaining - overhead);
        const auto revision = ++sent.am.clock;
        sent.am.entries.emplace(std::move(key),
                                AccountMetadataEntry {revision,
                                                      std::string(64, 'a'),
                                                      std::string(length, static_cast<char>('a' + revision % 26))});
        remaining -= overhead + length;
    }
    CPPUNIT_ASSERT_NO_THROW(AccountMetadataStore::validate(sent.am));
    msgpack::sbuffer large(UINT16_MAX);
    msgpack::pack(large, sent);
    CPPUNIT_ASSERT(large.size() > AccountMetadataStore::MAX_STATE_BYTES - AccountMetadataStore::MAX_VALUE_BYTES);

    LegacySyncMsg before;
    before.p["conversation"]["color"] = "blue";
    sent.p["conversation"]["color"] = "green";
    SyncMsg after;
    after.p["conversation"]["color"] = "red";
    const std::vector<SyncMsg> expected {repack<SyncMsg>(before), sent, after};

    // The production reader sees a byte stream, not multiplexed frame boundaries.
    // Exercise complete/coalesced objects, full-sized frames, and split headers/strings.
    msgpack::sbuffer stream(UINT16_MAX);
    msgpack::pack(stream, before);
    msgpack::pack(stream, sent);
    msgpack::pack(stream, after);
    const std::vector<std::vector<size_t>> chunkings {
        {UINT16_MAX},
        {size_t(UINT16_MAX) + 1},
        {1, 2, 3, 7, 4093, UINT16_MAX},
        {stream.size()},
    };
    for (const auto& chunks : chunkings) {
        size_t received = 0;
        auto reader = buildSyncMsgReader([&](SyncMsg&& msg) {
            CPPUNIT_ASSERT(received < expected.size());
            CPPUNIT_ASSERT(msg.am == expected[received].am);
            CPPUNIT_ASSERT(msg.p == expected[received].p);
            ++received;
            return std::error_code {};
        });
        size_t legacyReceived = 0;
        auto oldReader = dhtnet::buildMsgpackReader<LegacySyncMsg>([&](LegacySyncMsg&& msg) {
            CPPUNIT_ASSERT(legacyReceived < expected.size());
            CPPUNIT_ASSERT(msg.p == expected[legacyReceived].p);
            ++legacyReceived;
            return std::error_code {};
        });
        for (size_t offset = 0, chunk = 0; offset < stream.size(); ++chunk) {
            const auto length = std::min(chunks[chunk % chunks.size()], stream.size() - offset);
            auto bytes = reinterpret_cast<const uint8_t*>(stream.data() + offset);
            CPPUNIT_ASSERT_EQUAL(ssize_t(length), reader(bytes, length));
            CPPUNIT_ASSERT_EQUAL(ssize_t(length), oldReader(bytes, length));
            offset += length;
        }
        CPPUNIT_ASSERT_EQUAL(expected.size(), received);
        CPPUNIT_ASSERT_EQUAL(expected.size(), legacyReceived);
    }
}

void
ConvInfoSerializationTest::testAccountMetadataSyncMalformedStream()
{
    // Reserved MessagePack tag and an object of the wrong type both fail closed.
    for (uint8_t byte : {0xc1, 0xc0}) {
        size_t received = 0;
        auto reader = buildSyncMsgReader([&](SyncMsg&&) {
            ++received;
            return std::error_code {};
        });
        CPPUNIT_ASSERT(reader(&byte, 1) < 0);
        CPPUNIT_ASSERT_EQUAL(size_t(0), received);
    }
}

static void
packMetadataPrefix(msgpack::packer<msgpack::sbuffer>& packer, uint32_t entries)
{
    // Follow the build's configured SyncMsg layout; AccountMetadata is always map encoded.
    msgpack::sbuffer encoded;
    SyncMsg empty;
    msgpack::pack(encoded, empty);
    auto object = msgpack::unpack(encoded.data(), encoded.size());
    if (object.get().type == msgpack::type::ARRAY) {
        packer.pack_array(7);
        packer.pack(empty.ds);
        packer.pack(empty.c);
        packer.pack(empty.cr);
        packer.pack(empty.p);
        packer.pack(empty.ld);
        packer.pack(empty.ms);
    } else {
        packer.pack_map(1);
        packer.pack(std::string("am"));
    }
    packer.pack_map(2);
    packer.pack(std::string("clock"));
    packer.pack(uint64_t(1));
    packer.pack(std::string("entries"));
    packer.pack_map(entries);
}

void
ConvInfoSerializationTest::testSyncRejectsOversizedMetadataHeaders()
{
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> packer(buffer);
    packMetadataPrefix(packer, AccountMetadataStore::MAX_ENTRIES + 1);
    size_t delivered = 0;
    auto reader = buildSyncMsgReader([&](SyncMsg&&) {
        ++delivered;
        return std::error_code {};
    });
    // Only a declared count is sent, not thousands of entries to allocate/convert.
    CPPUNIT_ASSERT(buffer.size() < 256);
    CPPUNIT_ASSERT(reader(reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size()) < 0);
    CPPUNIT_ASSERT_EQUAL(size_t(0), delivered);
}

void
ConvInfoSerializationTest::testSyncRejectsOversizedMetadataStrings()
{
    for (bool oversizedKey : {false, true}) {
        SyncMsg sent;
        sent.am.clock = 1;
        const auto key = std::string(oversizedKey ? AccountMetadataStore::MAX_KEY_BYTES + 1 : 1, 'k');
        const auto value = std::string(oversizedKey ? 1 : AccountMetadataStore::MAX_VALUE_BYTES + 1, 'v');
        sent.am.entries[key] = {1, std::string(64, 'a'), value};
        msgpack::sbuffer buffer;
        msgpack::pack(buffer, sent);
        size_t delivered = 0;
        auto reader = buildSyncMsgReader([&](SyncMsg&&) {
            ++delivered;
            return std::error_code {};
        });
        CPPUNIT_ASSERT(reader(reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size()) < 0);
        CPPUNIT_ASSERT_EQUAL(size_t(0), delivered);
    }
}

void
ConvInfoSerializationTest::testSyncRejectsMetadataBudgetBeforeDelivery()
{
    SyncMsg sent;
    for (unsigned i = 0; i < 65; ++i)
        sent.am.entries[std::to_string(i)] = {++sent.am.clock,
                                              std::string(64, 'a'),
                                              std::string(AccountMetadataStore::MAX_VALUE_BYTES, 'v')};
    msgpack::sbuffer buffer;
    msgpack::pack(buffer, sent);
    CPPUNIT_ASSERT(buffer.size() < SyncMsgReadLimits::MAX_WIRE_BYTES);
    size_t delivered = 0;
    auto reader = buildSyncMsgReader([&](SyncMsg&&) {
        ++delivered;
        return std::error_code {};
    });
    bool rejected = false;
    for (size_t offset = 0; offset < buffer.size();) {
        auto length = std::min<size_t>(UINT16_MAX, buffer.size() - offset);
        if (reader(reinterpret_cast<const uint8_t*>(buffer.data() + offset), length) < 0) {
            rejected = true;
            break;
        }
        offset += length;
    }
    CPPUNIT_ASSERT(rejected);
    CPPUNIT_ASSERT_EQUAL(size_t(0), delivered);
}

void
ConvInfoSerializationTest::testSyncBoundsIncompleteReassembly()
{
    msgpack::sbuffer prefix;
    msgpack::packer<msgpack::sbuffer> packer(prefix);
    packMetadataPrefix(packer, 1);
    packer.pack(std::string("key"));
    packer.pack_map(3);
    packer.pack(std::string("revision"));
    packer.pack(uint64_t(1));
    packer.pack(std::string("writer"));
    packer.pack(std::string(64, 'a'));
    packer.pack(std::string("value"));
    packer.pack_str(static_cast<uint32_t>(SyncMsgReadLimits::MAX_WIRE_BYTES + 1));
    size_t delivered = 0;
    auto reader = buildSyncMsgReader([&](SyncMsg&&) {
        ++delivered;
        return std::error_code {};
    });
    auto result = reader(reinterpret_cast<const uint8_t*>(prefix.data()), prefix.size());
    const std::vector<uint8_t> chunk(65536, 'x');
    size_t supplied = prefix.size();
    while (result >= 0 && supplied <= SyncMsgReadLimits::MAX_WIRE_BYTES) {
        result = reader(chunk.data(), chunk.size());
        supplied += chunk.size();
    }
    CPPUNIT_ASSERT(result < 0);
    CPPUNIT_ASSERT(supplied <= SyncMsgReadLimits::MAX_WIRE_BYTES + chunk.size());
    CPPUNIT_ASSERT_EQUAL(size_t(0), delivered);
    msgpack::sbuffer valid;
    msgpack::pack(valid, SyncMsg {});
    CPPUNIT_ASSERT(reader(reinterpret_cast<const uint8_t*>(valid.data()), valid.size()) < 0);
    CPPUNIT_ASSERT_EQUAL(size_t(0), delivered);
}

void
ConvInfoSerializationTest::testSyncBoundsContainerHeadersAndDepth()
{
    // map32/array32 must not preallocate the advertised 2^32-1 children.
    for (const std::vector<uint8_t> bytes : {std::vector<uint8_t> {0xdf, 0xff, 0xff, 0xff, 0xff},
                                             std::vector<uint8_t> {0xdd, 0xff, 0xff, 0xff, 0xff},
                                             std::vector<uint8_t>(SyncMsgReadLimits::MAX_DEPTH + 1, 0x91)}) {
        size_t delivered = 0;
        auto reader = buildSyncMsgReader([&](SyncMsg&&) {
            ++delivered;
            return std::error_code {};
        });
        CPPUNIT_ASSERT(reader(bytes.data(), bytes.size()) < 0);
        CPPUNIT_ASSERT_EQUAL(size_t(0), delivered);
    }
}

void
ConvInfoSerializationTest::testSyncAcceptsCoalescedObjectsBeyondBufferLimit()
{
    // Metadata limits must not be applied as per-string limits to unrelated preference data.
    SyncMsg message;
    message.p["conversation"]["draft"] = std::string(SyncMsgReadLimits::MAX_WIRE_BYTES / 2, 'd');
    msgpack::sbuffer buffer;
    for (int i = 0; i < 3; ++i)
        msgpack::pack(buffer, message);
    CPPUNIT_ASSERT(buffer.size() > SyncMsgReadLimits::MAX_WIRE_BYTES);
    size_t delivered = 0;
    auto reader = buildSyncMsgReader([&](SyncMsg&& decoded) {
        CPPUNIT_ASSERT(decoded.p == message.p);
        ++delivered;
        return std::error_code {};
    });
    CPPUNIT_ASSERT_EQUAL(ssize_t(buffer.size()), reader(reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size()));
    CPPUNIT_ASSERT_EQUAL(size_t(3), delivered);
}

void
ConvInfoSerializationTest::testSyncBoundsAggregateObjectCount()
{
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> packer(buffer);
    // Declared child slots accumulate even though no large scalar bodies are supplied.
    const auto children = static_cast<uint32_t>(SyncMsgReadLimits::MAX_OBJECTS / 4);
    for (unsigned i = 0; i < 5; ++i)
        packer.pack_array(children);
    size_t delivered = 0;
    auto reader = buildSyncMsgReader([&](SyncMsg&&) {
        ++delivered;
        return std::error_code {};
    });
    CPPUNIT_ASSERT(buffer.size() < 32);
    CPPUNIT_ASSERT(reader(reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size()) < 0);
    CPPUNIT_ASSERT_EQUAL(size_t(0), delivered);
}

void
ConvInfoSerializationTest::testSyncStopsAfterApplicationError()
{
    msgpack::sbuffer buffer;
    msgpack::pack(buffer, SyncMsg {});
    msgpack::pack(buffer, SyncMsg {});
    size_t delivered = 0;
    auto reader = buildSyncMsgReader([&](SyncMsg&&) {
        ++delivered;
        return std::make_error_code(std::errc::invalid_argument);
    });
    auto bytes = reinterpret_cast<const uint8_t*>(buffer.data());
    CPPUNIT_ASSERT(reader(bytes, buffer.size()) < 0);
    CPPUNIT_ASSERT_EQUAL(size_t(1), delivered);
    CPPUNIT_ASSERT(reader(bytes, buffer.size()) < 0);
    CPPUNIT_ASSERT_EQUAL(size_t(1), delivered);
}

void
ConvInfoSerializationTest::testSyncResetsMetadataBudgetBetweenCoalescedMessages()
{
    SyncMsg message;
    for (unsigned i = 0; i < 48; ++i)
        message.am.entries[std::to_string(i)] = {++message.am.clock,
                                                 std::string(64, 'a'),
                                                 std::string(AccountMetadataStore::MAX_VALUE_BYTES, 'v')};
    AccountMetadataStore::validate(message.am);
    msgpack::sbuffer buffer;
    for (int i = 0; i < 3; ++i)
        msgpack::pack(buffer, message);
    CPPUNIT_ASSERT(buffer.size() > SyncMsgReadLimits::MAX_WIRE_BYTES);
    size_t delivered = 0;
    auto reader = buildSyncMsgReader([&](SyncMsg&& decoded) {
        CPPUNIT_ASSERT(decoded.am == message.am);
        ++delivered;
        return std::error_code {};
    });
    CPPUNIT_ASSERT_EQUAL(ssize_t(buffer.size()), reader(reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size()));
    CPPUNIT_ASSERT_EQUAL(size_t(3), delivered);
}

void
ConvInfoSerializationTest::testSyncPropagatesApplicationExceptionAndStops()
{
    msgpack::sbuffer buffer;
    msgpack::pack(buffer, SyncMsg {});
    size_t delivered = 0;
    auto reader = buildSyncMsgReader([&](SyncMsg&&) -> std::error_code {
        ++delivered;
        throw std::runtime_error("application failure");
    });
    const auto bytes = reinterpret_cast<const uint8_t*>(buffer.data());
    CPPUNIT_ASSERT_THROW(reader(bytes, buffer.size()), std::runtime_error);
    CPPUNIT_ASSERT_EQUAL(size_t(1), delivered);
    CPPUNIT_ASSERT(reader(bytes, buffer.size()) < 0);
    CPPUNIT_ASSERT_EQUAL(size_t(1), delivered);
}

void
ConvInfoSerializationTest::testSyncRejectsOversizedWriter()
{
    SyncMsg message;
    message.am.clock = 1;
    message.am.entries["key"] = {1, std::string(65536, 'a'), "value"};
    msgpack::sbuffer buffer;
    msgpack::pack(buffer, message);
    size_t delivered = 0;
    auto reader = buildSyncMsgReader([&](SyncMsg&&) {
        ++delivered;
        return std::error_code {};
    });
    CPPUNIT_ASSERT(reader(reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size()) < 0);
    CPPUNIT_ASSERT_EQUAL(size_t(0), delivered);
}

void
ConvInfoSerializationTest::testConvInfoLegacyToNew()
{
    LegacyConvInfo legacy;
    legacy.id = "conv1";
    legacy.created = 1700000001;
    legacy.removed = 1700000002;
    legacy.erased = 1700000003;
    legacy.members = {"alice", "bob"};
    legacy.lastDisplayed = "commitId";
    legacy.mode = ConversationMode::ONE_TO_ONE;

    auto info = repack<ConvInfo>(legacy);
    CPPUNIT_ASSERT_EQUAL(std::string("conv1"), info.id);
    CPPUNIT_ASSERT_EQUAL(int64_t(1700000001000), toMillisecondsSinceEpoch(info.created));
    CPPUNIT_ASSERT_EQUAL(int64_t(1700000002000), toMillisecondsSinceEpoch(info.removed));
    CPPUNIT_ASSERT_EQUAL(int64_t(1700000003000), toMillisecondsSinceEpoch(info.erased));
    CPPUNIT_ASSERT(info.members == legacy.members);
    CPPUNIT_ASSERT_EQUAL(std::string("commitId"), info.lastDisplayed);
    CPPUNIT_ASSERT(info.mode == ConversationMode::ONE_TO_ONE);
}

void
ConvInfoSerializationTest::testConvInfoNewToLegacy()
{
    ConvInfo info(std::string("conv1"));
    info.created = timePointFromMilliseconds(1700000001123);
    info.removed = timePointFromMilliseconds(1700000002456);
    info.erased = timePointFromMilliseconds(1700000003789);
    info.members = {"alice"};
    info.lastDisplayed = "commitId";
    info.mode = ConversationMode::INVITES_ONLY;

    auto legacy = repack<LegacyConvInfo>(info);
    CPPUNIT_ASSERT_EQUAL(std::string("conv1"), legacy.id);
    CPPUNIT_ASSERT_EQUAL(time_t(1700000001), legacy.created);
    CPPUNIT_ASSERT_EQUAL(time_t(1700000002), legacy.removed);
    CPPUNIT_ASSERT_EQUAL(time_t(1700000003), legacy.erased);
    CPPUNIT_ASSERT(legacy.members == info.members);
    CPPUNIT_ASSERT_EQUAL(std::string("commitId"), legacy.lastDisplayed);
    CPPUNIT_ASSERT(legacy.mode == ConversationMode::INVITES_ONLY);
}

void
ConvInfoSerializationTest::testConvInfoMsgpackRoundtrip()
{
    ConvInfo info(std::string("conv1"));
    info.created = timePointFromMilliseconds(1700000001123);
    info.removed = timePointFromMilliseconds(1700000001124);
    info.members = {"alice", "bob"};

    auto out = repack<ConvInfo>(info);
    CPPUNIT_ASSERT(out.created == info.created);
    CPPUNIT_ASSERT(out.removed == info.removed);
    CPPUNIT_ASSERT(out.erased == info.erased);
    CPPUNIT_ASSERT(out.members == info.members);
}

void
ConvInfoSerializationTest::testConvInfoJson()
{
    ConvInfo info(std::string("conv1"));
    info.created = timePointFromMilliseconds(1700000001123);
    info.removed = timePointFromMilliseconds(1700000002456);
    info.members = {"alice"};

    // Round-trip via JSON preserves ms
    ConvInfo fromJson(info.toJson());
    CPPUNIT_ASSERT(fromJson.created == info.created);
    CPPUNIT_ASSERT(fromJson.removed == info.removed);

    // Legacy JSON (seconds only, e.g. an old account archive) still loads
    Json::Value legacy;
    legacy[ConversationMapKeys::ID] = "conv1";
    legacy[ConversationMapKeys::CREATED] = Json::Int64(1700000001);
    legacy[ConversationMapKeys::REMOVED] = Json::Int64(1700000002);
    ConvInfo fromLegacy(legacy);
    CPPUNIT_ASSERT_EQUAL(int64_t(1700000001000), toMillisecondsSinceEpoch(fromLegacy.created));
    CPPUNIT_ASSERT_EQUAL(int64_t(1700000002000), toMillisecondsSinceEpoch(fromLegacy.removed));
    CPPUNIT_ASSERT_EQUAL(int64_t(0), toMillisecondsSinceEpoch(fromLegacy.erased));
}

void
ConvInfoSerializationTest::testRequestLegacyToNew()
{
    LegacyConversationRequest legacy;
    legacy.conversationId = "conv1";
    legacy.from = "alice";
    legacy.metadatas = {{"mode", "0"}};
    legacy.received = 1700000001;
    legacy.declined = 1700000002;

    auto req = repack<ConversationRequest>(legacy);
    CPPUNIT_ASSERT_EQUAL(std::string("conv1"), req.conversationId);
    CPPUNIT_ASSERT_EQUAL(std::string("alice"), req.from);
    CPPUNIT_ASSERT(req.metadatas == legacy.metadatas);
    CPPUNIT_ASSERT_EQUAL(int64_t(1700000001000), toMillisecondsSinceEpoch(req.received));
    CPPUNIT_ASSERT_EQUAL(int64_t(1700000002000), toMillisecondsSinceEpoch(req.declined));
}

void
ConvInfoSerializationTest::testRequestNewToLegacy()
{
    ConversationRequest req;
    req.conversationId = "conv1";
    req.from = "alice";
    req.received = timePointFromMilliseconds(1700000001123);
    req.declined = timePointFromMilliseconds(1700000002456);

    auto legacy = repack<LegacyConversationRequest>(req);
    CPPUNIT_ASSERT_EQUAL(std::string("conv1"), legacy.conversationId);
    CPPUNIT_ASSERT_EQUAL(std::string("alice"), legacy.from);
    CPPUNIT_ASSERT_EQUAL(time_t(1700000001), legacy.received);
    CPPUNIT_ASSERT_EQUAL(time_t(1700000002), legacy.declined);
}

void
ConvInfoSerializationTest::testRequestMsgpackRoundtrip()
{
    ConversationRequest req;
    req.conversationId = "conv1";
    req.from = "alice";
    req.metadatas = {{"title", "test"}};
    req.received = timePointFromMilliseconds(1700000001123);

    auto out = repack<ConversationRequest>(req);
    CPPUNIT_ASSERT(out.received == req.received);
    CPPUNIT_ASSERT(out.declined == req.declined);
    CPPUNIT_ASSERT(out.metadatas == req.metadatas);
}

void
ConvInfoSerializationTest::testRequestToMapStaysSeconds()
{
    ConversationRequest req;
    req.conversationId = "conv1";
    req.from = "alice";
    req.received = timePointFromMilliseconds(1700000001123);
    req.declined = timePointFromMilliseconds(1700000002456);

    auto map = req.toMap();
    CPPUNIT_ASSERT_EQUAL(std::string("1700000001"), map.at(ConversationMapKeys::RECEIVED));
    CPPUNIT_ASSERT_EQUAL(std::string("1700000002"), map.at(ConversationMapKeys::DECLINED));
}

void
ConvInfoSerializationTest::testIsRemovedMsResolution()
{
    ConvInfo info(std::string("conv1"));
    info.created = timePointFromMilliseconds(1700000001500);
    // Removed 1 ms later, within the same second: now detectable
    info.removed = timePointFromMilliseconds(1700000001501);
    CPPUNIT_ASSERT(info.isRemoved());
    // Re-created 1 ms after removal
    info.created = timePointFromMilliseconds(1700000001502);
    CPPUNIT_ASSERT(!info.isRemoved());
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::ConvInfoSerializationTest::name());
